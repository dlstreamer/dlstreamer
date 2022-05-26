/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensorconverter.hpp"

#include <capabilities/tensor_caps.hpp>
#include <gst/video/video-format.h>
#include <gva_caps.h>
#include <meta/gva_buffer_flags.hpp>
#include <scope_guard.h>
#include <tensor_layer_desc.hpp>
#include <utils.h>

#include <dlstreamer/buffer_info.h>
#include <dlstreamer/gst/utils.h>

#ifdef ENABLE_VAAPI
#include <dlstreamer/gst/vaapi_context.h>
#include <vaapi_image_info.hpp> // FIXME: should be removed once switched to new BufferMapper in inference element
#endif

#define GVA_TENSOR_CONV_VIDEO_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR }") "; " VASURFACE_CAPS
#define GVA_TENSOR_CONV_TENSOR_CAPS GVA_TENSORS_CAPS GVA_VAAPI_TENSORS_CAPS

GST_DEBUG_CATEGORY(gva_tensor_conv_debug);
#define GST_CAT_DEFAULT gva_tensor_conv_debug

class GvaTensorConverterPrivate {
  public:
    static GvaTensorConverterPrivate *unpack(GstBaseTransform *base) {
        g_assert(GVA_TENSOR_CONVERTER(base)->impl);
        return GVA_TENSOR_CONVERTER(base)->impl;
    }

    explicit GvaTensorConverterPrivate(GstBaseTransform *base) : _base(base) {
    }

    ~GvaTensorConverterPrivate() {
        if (_in_caps)
            gst_caps_unref(_in_caps);
        if (_out_caps)
            gst_caps_unref(_out_caps);
    }

    // BaseTransform set_caps overload
    bool set_caps(GstCaps *incaps, GstCaps *outcaps) {
        if (gst_caps_get_size(incaps) > 1 || gst_caps_get_size(outcaps) > 1) {
            GST_ERROR_OBJECT(_base, "Only single capabilities on each pad is supported.");
            return false;
        }

        TensorCaps tensor_caps;
        try {
            tensor_caps = TensorCaps::FromCaps(outcaps);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }

        if (_in_caps)
            gst_caps_unref(_in_caps);
        if (_out_caps)
            gst_caps_unref(_out_caps);
        _in_caps = gst_caps_copy(incaps);
        _out_caps = gst_caps_copy(outcaps);

        if (tensor_caps.GetMemoryType() == InferenceBackend::MemoryType::VAAPI) {
            try {
                configure_vaapi_sharing_for_inference();
            } catch (const std::exception &e) {
                GST_ERROR_OBJECT(_base, "Failed to configure VAAPI surface sharing: %s",
                                 Utils::createNestedErrorMsg(e).c_str());
                return false;
            }
        } else {
            try {
                configure_ie_preprocessing_for_inference();
            } catch (const std::exception &e) {
                GST_ERROR_OBJECT(_base, "Failed to configure IE preprocessing: %s",
                                 Utils::createNestedErrorMsg(e).c_str());
                return false;
            }
        }

        return true;
    }

    // BaseTransform transform_caps overload
    GstCaps *transform_caps(GstPadDirection direction, GstCaps *caps, GstCaps *filter) {
        g_assert(direction == GST_PAD_SINK || direction == GST_PAD_SRC);
        GST_DEBUG_OBJECT(_base, "Transform caps: DIRECTION: %u; CAPS: %" GST_PTR_FORMAT "; FILTER: %" GST_PTR_FORMAT,
                         direction, caps, filter);

        if (!_caps_ready) {
            _caps_ready = prepare_allowed_caps();
            _caps_ready = _caps_ready || prepare_allowed_caps_based_on_downstream(caps);
        }

        GstCaps *target_caps;
        if (direction == GST_PAD_SINK)
            target_caps = transform_in_caps(caps);
        else
            target_caps = transform_out_caps(caps);

        GST_DEBUG_OBJECT(_base, "Transformed: %" GST_PTR_FORMAT, target_caps);

        if (filter) {
            GstCaps *intersection = gst_caps_intersect_full(filter, target_caps, GST_CAPS_INTERSECT_FIRST);
            gst_caps_unref(target_caps);
            target_caps = intersection;

            GST_DEBUG_OBJECT(_base, "Filtered: %" GST_PTR_FORMAT, target_caps);
        }

        return target_caps;
    }

    // BaseTransform transform_ip overload
    // FIXME: should be removed once switched to new BufferMapper in inference element
    // The purpose of the code below is to properly set `VaapiImageInfo` for the buffer (specifically VASurfaceID),
    // so that inference element can get it.
    GstFlowReturn transform_ip(GstBuffer *buf) {
        if (!_vaapi_context) {
            return GST_FLOW_OK;
        }

#ifdef ENABLE_VAAPI
        if (gst_mini_object_get_qdata(&buf->mini_object, g_quark_from_static_string("VaApiImage")))
            return GST_FLOW_OK;

        auto image = new InferenceBackend::VaApiImage;
        image->image_map.reset(InferenceBackend::ImageMap::Create(InferenceBackend::MemoryType::VAAPI));
        image->image.va_surface_id = get_va_surface(buf);

        auto info = new VaapiImageInfo{nullptr, image, {}};
        gst_mini_object_set_qdata(&buf->mini_object, g_quark_from_static_string("VaApiImage"), info, [](gpointer data) {
            auto info = reinterpret_cast<VaapiImageInfo *>(data);
            if (!info)
                return;
            if (info->image)
                info->image->Unmap();
            delete info->image;
            delete info;
        });
#else
        (void)buf;
#endif

        return GST_FLOW_OK;
    }

    const TensorLayerDesc &get_model_input() {
        if (!_model_input)
            query_model_input_internal();
        return _model_input;
    }

  private:
    // Creates caps filter with specified media type based on caps features contained in `base` caps.
    static GstCaps *create_features_filter(GstCaps *base, const char *media_type) {
        GstCaps *filter = gst_caps_new_empty();
        guint size = gst_caps_get_size(base);
        for (guint i = 0; i < size; i++) {
            auto features = gst_caps_get_features(base, i);
            auto caps = gst_caps_new_empty_simple(media_type);
            gst_caps_set_features_simple(caps, gst_caps_features_copy(features));
            gst_caps_append(filter, caps);
        }

        return filter;
    }

    // Tranform caps on sink pad to caps on source pad
    GstCaps *transform_in_caps(GstCaps *in_caps) {
        if (!_in_caps || !_out_caps)
            return get_template_caps_copy(GST_PAD_SRC);

        // Test if input caps can be intersected with currently allowed caps
        GstCaps *intersection = gst_caps_intersect(in_caps, _in_caps);

        // If not - we cannot transform such caps
        if (gst_caps_is_empty(intersection)) {
            GST_INFO_OBJECT(_base, "Sink caps cannot be transformed: %" GST_PTR_FORMAT, in_caps);
            GST_INFO_OBJECT(_base, "Allowed sink caps are: %" GST_PTR_FORMAT, _in_caps);
            return intersection;
        }

        // Create caps filter base on input caps features
        auto ffilter = create_features_filter(intersection, GVA_TENSOR_MEDIA_NAME);

        // Don't forget to release the memory
        auto sg = makeScopeGuard([&] {
            gst_caps_unref(intersection);
            gst_caps_unref(ffilter);
        });

        GST_INFO_OBJECT(_base, "Features filter: %" GST_PTR_FORMAT, ffilter);
        return gst_caps_intersect(ffilter, _out_caps);
    }

    // Tranform caps on source pad to caps on sink pad
    GstCaps *transform_out_caps(GstCaps *out_caps) {
        if (!_in_caps)
            return get_template_caps_copy(GST_PAD_SINK);

        auto ffilter = create_features_filter(out_caps, "video/x-raw");
        auto sg = makeScopeGuard([&] { gst_caps_unref(ffilter); });
        GST_INFO_OBJECT(_base, "Features filter: %" GST_PTR_FORMAT, ffilter);

        return gst_caps_intersect(ffilter, _in_caps);
    }

    void query_model_input_internal() {
        auto query = gva_query_new_model_input();
        auto sg_query = makeScopeGuard([&] { gst_query_unref(query); });

        if (!gst_pad_peer_query(GST_BASE_TRANSFORM_SRC_PAD(_base), query))
            return;

        if (!gva_query_parse_model_input(query, _model_input))
            _model_input = {};
    }

    // Returns a copy of pad template caps of specified direction
    GstCaps *get_template_caps_copy(GstPadDirection direction) const {
        assert(direction == GST_PAD_SINK || direction == GST_PAD_SRC);

        GstPad *pad =
            direction == GST_PAD_SINK ? GST_BASE_TRANSFORM_SINK_PAD(_base) : GST_BASE_TRANSFORM_SRC_PAD(_base);
        GstCaps *template_caps = gst_pad_get_pad_template_caps(pad);
        GstCaps *copy = gst_caps_copy(template_caps);
        gst_caps_unref(template_caps);

        return copy;
    }

    // Build allowed caps based on model input query
    bool prepare_allowed_caps() {
        assert(!_in_caps);
        assert(!_out_caps);

        auto &model_in = get_model_input();
        if (!model_in)
            return false;

        try {
            TensorCaps tensor_caps;
            tensor_caps =
                TensorCaps(InferenceBackend::MemoryType::SYSTEM, model_in.precision, model_in.layout, model_in.dims);

            auto map_fn = [](GstCapsFeatures *, GstStructure *structure, gpointer user_data) -> gboolean {
                auto tensor = static_cast<TensorCaps *>(user_data);
                return TensorCaps::ToStructure(*tensor, structure);
            };

            // Fixate some fields based on model input
            auto src_caps = get_template_caps_copy(GST_PAD_SRC);
            if (!gst_caps_map_in_place(src_caps, map_fn, &tensor_caps)) {
                gst_caps_unref(src_caps);
                return false;
            }
            _out_caps = src_caps;

            _in_caps = get_template_caps_copy(GST_PAD_SINK);
            gst_caps_set_simple(_in_caps, "width", G_TYPE_INT, tensor_caps.GetWidth(), "height", G_TYPE_INT,
                                tensor_caps.GetHeight(), nullptr);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failed to create tensor caps: %s", e.what());
            return false;
        }

        GST_INFO_OBJECT(_base, "Allowed SINK caps: %" GST_PTR_FORMAT, _in_caps);
        GST_INFO_OBJECT(_base, "Allowed SRC caps: %" GST_PTR_FORMAT, _out_caps);
        return true;
    }

    // Build allowed caps based on downstream caps
    bool prepare_allowed_caps_based_on_downstream(GstCaps *caps) {
        assert(!_in_caps);
        assert(!_out_caps);

        GstCaps *result = nullptr;

        std::set<guint> picked;
        using namespace dlstreamer;
        for (guint i = 0; i < gst_caps_get_size(caps); i++) {
            BufferInfo info = gst_caps_to_buffer_info(caps, i);
            if (info.planes.empty())
                continue;
            if (info.media_type != MediaType::TENSORS)
                continue;
            GstStructure *structure = gst_caps_get_structure(caps, i);
            // FIXME: caps with more than 1 shape
            if (info.planes.size() > 1) {
                GST_WARNING_OBJECT(_base, "Multiple tensors are not suppoted: %" GST_PTR_FORMAT, structure);
                continue;
            }

            GST_INFO_OBJECT(_base, "TENSOR CAPS with dims: %" GST_PTR_FORMAT, structure);

            if (info.planes.front().layout != dlstreamer::Layout::NHWC) {
                // HCHW can be supported once moved to GStreamer 1.20
                continue;
            }

            GstCaps *c = get_template_caps_copy(GST_PAD_SINK);
            gst_caps_set_simple(c, "width", G_TYPE_INT, info.planes.front().width(), "height", G_TYPE_INT,
                                info.planes.front().height(), nullptr);
            // FIXME: channels -> formats, memory type

            if (result)
                result = gst_caps_merge(result, c);
            else
                result = c;
            picked.emplace(i);
        }

        if (!result)
            return false;

        _in_caps = result;
        _out_caps = gst_caps_copy(caps);
        for (guint i = gst_caps_get_size(_out_caps); i != 0; i--) {
            if (picked.count(i - 1) == 0)
                gst_caps_remove_structure(_out_caps, i - 1);
        }

        return true;
    }

#ifdef ENABLE_VAAPI
    static unsigned int get_va_surface(GstBuffer *buffer) {
        auto flags = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);
        GstMapInfo map_info;
        gboolean sts = gst_buffer_map(buffer, &map_info, flags);
        if (!sts) {
            flags = static_cast<GstMapFlags>(flags | GST_MAP_READ);
            sts = gst_buffer_map(buffer, &map_info, flags);
        }
        if (sts) {
            unsigned int va_surface_id = *reinterpret_cast<uint32_t *>(map_info.data);
            gst_buffer_unmap(buffer, &map_info);

            return va_surface_id;
        }
        return -1;
    }
#endif

    void configure_vaapi_sharing_for_inference() {
#ifdef ENABLE_VAAPI
        auto display = std::make_shared<dlstreamer::GSTVAAPIContext>(_base);
        if (!display)
            throw std::runtime_error("Failed to get VAAPI Context");
        GST_DEBUG_OBJECT(_base, "Got VADisplay %p", display.get());

        auto event = gva_event_new_preproc_info(nullptr, InferenceEngine::ResizeAlgorithm::NO_RESIZE,
                                                InferenceEngine::ColorFormat::NV12, display->va_display());
        if (!gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(_base), event))
            throw std::runtime_error("Couldn't send VAAPI pre-processing event");

        // Keep reference to context
        _vaapi_context = std::move(display);
#endif
    }

    void configure_ie_preprocessing_for_inference() {
        // Enable ie preprocessing for gvatensorinference
        // TODO: necessary because we need to provide RGBP format to inference,
        // but it is unavailable till GStreamer 1.20
        // So we enable IE preprocssing to do required color space conversion
        // from BGRx to BGRP
        auto video_info = gst_video_info_new();
        auto guard = makeScopeGuard([video_info] { gst_video_info_free(video_info); });

        if (!gst_video_info_from_caps(video_info, _in_caps))
            throw std::runtime_error("Failed to construct video info from input caps");

        auto color_format = InferenceEngine::ColorFormat::BGR;
        switch (GST_VIDEO_INFO_FORMAT(video_info)) {
        case GST_VIDEO_FORMAT_BGR:
            color_format = InferenceEngine::BGR;
            break;
        case GST_VIDEO_FORMAT_BGRA:
        case GST_VIDEO_FORMAT_BGRx:
            color_format = InferenceEngine::BGRX;
            break;
        default:
            throw std::runtime_error(std::string("Unsupported input video format for IE preprocessing: ") +
                                     gst_video_format_to_string(GST_VIDEO_INFO_FORMAT(video_info)));
        }

        auto event = gva_event_new_preproc_info(video_info, InferenceEngine::ResizeAlgorithm::RESIZE_BILINEAR,
                                                color_format, nullptr);
        if (!gst_pad_push_event(_base->srcpad, event))
            throw std::runtime_error("Couldn't send ie pre-processing event");
    }

  private:
    GstBaseTransform *_base;
    TensorLayerDesc _model_input;
    bool _caps_ready = false;
    GstCaps *_in_caps = nullptr;
    GstCaps *_out_caps = nullptr;
    dlstreamer::ContextPtr _vaapi_context;
};

G_DEFINE_TYPE_WITH_PRIVATE(GvaTensorConverter, gva_tensor_conv, GST_TYPE_BASE_TRANSFORM);

static void gva_tensor_conv_init(GvaTensorConverter *self) {
    // FIXME: uncomment once moved to new BufferMapper in inference element
    // gst_base_transform_set_passthrough(&self->base, true);

    // Initialize of private data
    auto *priv_memory = gva_tensor_conv_get_instance_private(self);
    self->impl = new (priv_memory) GvaTensorConverterPrivate(&self->base);
}

static void gva_tensor_conv_finalize(GObject *object) {
    auto self = GVA_TENSOR_CONVERTER(object);

    if (self->impl) {
        // Manually invoke object destruction since it was created via placement-new.
        self->impl->~GvaTensorConverterPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_tensor_conv_parent_class)->finalize(object);
}

static void gva_tensor_conv_class_init(GvaTensorConverterClass *klass) {
    GST_DEBUG_CATEGORY_INIT(gva_tensor_conv_debug, "gvatensorconverter", 0, "GVA Tensor Converter");

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->transform_ip = [](GstBaseTransform *base, GstBuffer *buf) {
        return GvaTensorConverterPrivate::unpack(base)->transform_ip(buf);
    };
    base_transform_class->transform_caps = [](GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                              GstCaps *filter) {
        return GvaTensorConverterPrivate::unpack(base)->transform_caps(direction, caps, filter);
    };
    base_transform_class->set_caps = [](GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) -> gboolean {
        return GvaTensorConverterPrivate::unpack(base)->set_caps(incaps, outcaps);
    };
    G_OBJECT_CLASS(klass)->finalize = gva_tensor_conv_finalize;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, "[Preview] Tensor Converter", "Converter/Tensor",
                                          "Converts mediatype from video to tensor", "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_TENSOR_CONV_TENSOR_CAPS)));
    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_TENSOR_CONV_VIDEO_CAPS)));
}
