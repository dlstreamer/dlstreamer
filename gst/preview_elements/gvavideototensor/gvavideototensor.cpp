/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvavideototensor.hpp"
#include "preprocessors/ie_preproc.hpp"
#include "preprocessors/opencv_preproc.hpp"
#include "preprocessors/vaapi_preproc.hpp"
#include "preprocessors/vaapi_surface_sharing_preproc.hpp"

#include <capabilities/capabilities.hpp>
#include <gst_vaapi_helper.h>
#include <gva_custom_meta.hpp>
#include <model_proc_provider.h>
#include <pre_processor_info_parser.hpp>
#include <utils.h>

#include <gst/video/video.h>

#ifdef ENABLE_VAAPI
#include "vaapi_utils.h"
#endif

GST_DEBUG_CATEGORY(gst_gva_video_to_tensor_debug_category);

G_DEFINE_TYPE(GstGvaVideoToTensor, gst_gva_video_to_tensor, GST_TYPE_BASE_TRANSFORM);

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR, NV12, I420 }") "; "
#ifdef ENABLE_VAAPI
#define VASURFACE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES(VASURFACE_FEATURE_STR, "{ NV12 }") "; "
#else
#define VASURFACE_CAPS
#endif
#if (defined ENABLE_VPUX || defined ENABLE_VAAPI)
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES(DMABUF_FEATURE_STR, "{ NV12, RGBA, I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif
#define GVA_VIDEO_CAPS SYSTEM_MEM_CAPS DMA_BUFFER_CAPS VASURFACE_CAPS

#ifdef ENABLE_VAAPI
#define GVA_OUTPUT_CAPS GVA_TENSOR_CAPS GVA_VAAPI_TENSOR_CAPS
#else
#define GVA_OUTPUT_CAPS GVA_TENSOR_CAPS
#endif

#define GST_TYPE_GVA_VIDEO_TO_TENSOR_BACKEND (gst_gva_video_to_tensor_backend_get_type())

static GType gst_gva_video_to_tensor_backend_get_type(void) {
    static GType gva_video_to_tensor_backend_type = 0;
    static const GEnumValue backend_types[] = {
        {OPENCV, "OpenCV", "opencv"},
        {IE, "Inference Engine", "ie"},
#ifdef ENABLE_VAAPI
        {VAAPI_SYSTEM, "VAAPI", "vaapi"},
        {VAAPI_SURFACE_SHARING, "VAAPI Surface Sharing", "vaapi-surface-sharing"},
#endif
        {0, NULL, NULL}};

    if (!gva_video_to_tensor_backend_type) {
        gva_video_to_tensor_backend_type = g_enum_register_static("GstGVAVideoToTensorBackend", backend_types);
    }

    return gva_video_to_tensor_backend_type;
}

enum { PROP_0, PROP_MODEL_PROC, PROP_PRE_PROC_BACKEND };

using namespace InferenceBackend;

namespace {
constexpr auto DEFAULT_PRE_PROC_BACKEND = OPENCV;

VaApiDisplayPtr createVaDisplay(GstBaseTransform *base_transform) {
    assert(base_transform);

    auto display = VaapiHelper::queryVaDisplay(base_transform);
    if (display) {
        GST_DEBUG_OBJECT(base_transform, "Using shared VADisplay");
        return display;
    }

#ifdef ENABLE_VAAPI
    uint32_t rel_dev_index = 0;
    display = vaApiCreateVaDisplay(rel_dev_index);
#endif

    return display;
}

} // namespace

void _GstGvaVideoToTensor::init_preprocessor() {
    props.preprocessor.reset();

    switch (props.pre_proc_backend) {
    case OPENCV:
        props.preprocessor.reset(new OpenCVPreProc(props.input_info, props.tensor_caps, props.pre_proc_info));
        break;
    case IE:
        props.preprocessor.reset(new IEPreProc(props.input_info));
        break;
#ifdef ENABLE_VAAPI
    case VAAPI_SYSTEM:
        props.preprocessor.reset(new VaapiPreProc(createVaDisplay(&this->base), props.input_info, props.tensor_caps));
        break;
    case VAAPI_SURFACE_SHARING:
        props.preprocessor.reset(
            new VaapiSurfaceSharingPreProc(createVaDisplay(&this->base), props.input_info, props.tensor_caps));
        break;
#endif
    default:
        throw std::runtime_error("Unsupported preprocessor type");
    }
}

bool _GstGvaVideoToTensor::need_preprocessing() const {
    switch (props.pre_proc_backend) {
    case OPENCV:
        return props.pre_proc_info && props.pre_proc_info->isDefined();
    case IE:
        return true;
#ifdef ENABLE_VAAPI
    case VAAPI_SYSTEM:
    case VAAPI_SURFACE_SHARING:
        return true;
#endif
    default:
        GST_ERROR_OBJECT(this, "Unknown preprocessing backend. Skip preprocessing");
        return false;
    }
}

InferenceBackend::MemoryType
_GstGvaVideoToTensor::get_output_mem_type(InferenceBackend::MemoryType input_mem_type) const {
    if (input_mem_type == InferenceBackend::MemoryType::SYSTEM && props.pre_proc_backend != IE &&
        props.pre_proc_backend != OPENCV)
        throw std::invalid_argument("Only ie and opencv preprocessors supported for system memory");

    switch (props.pre_proc_backend) {
    case OPENCV:
    case IE:
        return InferenceBackend::MemoryType::SYSTEM;
#ifdef ENABLE_VAAPI
    case VAAPI_SYSTEM:
        return InferenceBackend::MemoryType::SYSTEM;
    case VAAPI_SURFACE_SHARING:
        return input_mem_type;
#endif
    default:
        throw std::invalid_argument("Unknown memory type");
    }
}

static void gst_gva_video_to_tensor_init(GstGvaVideoToTensor *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GstGvaVideoToTensor::_Props();

    self->props.pre_proc_backend = DEFAULT_PRE_PROC_BACKEND;
}

static void gst_gva_video_to_tensor_set_property(GObject *object, guint prop_id, const GValue *value,
                                                 GParamSpec *pspec) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        self->props.model_proc = g_value_get_string(value);
        break;
    case PROP_PRE_PROC_BACKEND:
        self->props.pre_proc_backend = static_cast<PreProcBackend>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_video_to_tensor_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        g_value_set_string(value, self->props.model_proc.c_str());
        break;
    case PROP_PRE_PROC_BACKEND:
        g_value_set_enum(value, self->props.pre_proc_backend);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_video_to_tensor_dispose(GObject *object) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gst_gva_video_to_tensor_parent_class)->dispose(object);
}

static void gst_gva_video_to_tensor_finalize(GObject *object) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    gst_video_info_free(self->props.input_info);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gst_gva_video_to_tensor_parent_class)->finalize(object);
}

static gboolean gst_gva_video_to_tensor_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (gst_caps_get_size(incaps) > 1 || gst_caps_get_size(outcaps) > 1) {
        GST_ERROR_OBJECT(self, "Only single capabilities on each pad is supported.");
        return false;
    }

    /* set input tensor info */
    if (!self->props.input_info) {
        self->props.input_info = gst_video_info_new();
    }
    gst_video_info_from_caps(self->props.input_info, incaps);

    try {
        self->props.tensor_caps = TensorCaps(outcaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        self->init_preprocessor();
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to create preprocessor: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    gst_base_transform_set_passthrough(base, !self->need_preprocessing());

    return true;
}

static gboolean gst_gva_video_to_tensor_sink_event(GstBaseTransform *base, GstEvent *event) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return GST_BASE_TRANSFORM_CLASS(gst_gva_video_to_tensor_parent_class)->sink_event(base, event);
}

static gboolean gst_gva_video_to_tensor_start(GstBaseTransform *base) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (!self->props.model_proc.empty()) {
        try {
            ModelProcProvider model_proc_provider;
            model_proc_provider.readJsonFile(self->props.model_proc);
            self->props.input_processor_info = model_proc_provider.parseInputPreproc();
            for (const auto &i : self->props.input_processor_info) {
                if (i->format == "image") {
                    self->props.pre_proc_info = PreProcParamsParser(i->params).parse();
                    break;
                }
            }
            // TODO: do we need warning user if no input provided in model-proc
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(self, "Failed to parse model proc file: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }
    }

    // For IE we don't need to modify buffer memory, only attach meta, so use transform_ip
    // TODO: refactor
    gst_base_transform_set_in_place(base, self->props.pre_proc_backend == IE);

    return true;
}

static gboolean gst_gva_video_to_tensor_stop(GstBaseTransform *base) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

GstCaps *gst_gva_video_to_tensor_transform_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                                GstCaps *filter) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GstCaps *ret = nullptr;

    auto srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(base));
    auto sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(base));

    switch (direction) {
    case GST_PAD_SINK: {
        if (gst_caps_can_intersect(caps, sinkcaps)) {
            auto mem_type = self->get_output_mem_type(get_memory_type_from_caps(caps));
            ret = (mem_type == InferenceBackend::MemoryType::SYSTEM) ? gst_caps_from_string(GVA_TENSOR_CAPS)
                                                                     : gst_caps_from_string(GVA_VAAPI_TENSOR_CAPS);
        } else {
            ret = gst_caps_new_empty();
        }
        break;
    }
    case GST_PAD_SRC: {
        if (gst_caps_can_intersect(caps, srccaps))
            ret = gst_caps_copy(sinkcaps);
        else
            ret = gst_caps_new_empty();
        break;
    }
    default:
        g_assert_not_reached();
    }

    GST_DEBUG_OBJECT(base, "transformed %" GST_PTR_FORMAT, ret);

    if (filter) {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(base, "Using filter caps %" GST_PTR_FORMAT, filter);

        intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;

        GST_DEBUG_OBJECT(base, "Intersection %" GST_PTR_FORMAT, ret);
    }

    gst_caps_unref(srccaps);
    gst_caps_unref(sinkcaps);

    return ret;
}

static GstFlowReturn gst_gva_video_to_tensor_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // If preprocessing is not needed then basetranform should work in passthrough mode
    g_assert(self->need_preprocessing());

    if (!self->props.preprocessor) {
        GST_ERROR_OBJECT(self, "Preprocessor is not initialized");
        return GST_FLOW_ERROR;
    }

    try {
        self->props.preprocessor->process(inbuf, outbuf);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during transforming input buffer: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static GstFlowReturn gst_gva_video_to_tensor_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // If preprocessing is not needed then basetranform should work in passthrough mode
    g_assert(self->need_preprocessing());

    if (!self->props.preprocessor) {
        GST_ERROR_OBJECT(self, "Preprocessor is not initialized");
        return GST_FLOW_ERROR;
    }

    try {
        self->props.preprocessor->process(buf);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during transforming input buffer: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static gboolean gst_gva_video_to_tensor_transform_size(GstBaseTransform *base, GstPadDirection /*direction*/,
                                                       GstCaps * /*caps*/, gsize size, GstCaps * /*othercaps*/,
                                                       gsize *othersize) {
    GstGvaVideoToTensor *self = GST_GVA_VIDEO_TO_TENSOR(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // transform_size shouldn't be called in this case
    g_assert(gst_base_transform_is_passthrough(base) == false);
    g_assert(gst_base_transform_is_in_place(base) == false);

    if (!self->props.input_info) {
        GST_ERROR_OBJECT(self, "Failed to calculate out buffer size: Input video info is not initialized");
        return false;
    }

    if (self->props.tensor_caps.GetMemoryType() == InferenceBackend::MemoryType::VAAPI) {
        *othersize = 0;
        return true;
    }

    gsize out_size = 0;
    try {
        auto out_width = self->props.tensor_caps.GetDimension(1);
        auto out_height = self->props.tensor_caps.GetDimension(2);
        auto format = gst_format_to_four_CC(GST_VIDEO_INFO_FORMAT(self->props.input_info));

        if (self->props.pre_proc_info && self->props.pre_proc_info->doNeedColorSpaceConversion(format)) {
            switch (self->props.pre_proc_info->getTargetColorSpace()) {
            case InputImageLayerDesc::ColorSpace::RGB:
            case InputImageLayerDesc::ColorSpace::BGR:
                out_size = 3 * out_width * out_height;
                break;
            case InputImageLayerDesc::ColorSpace::GRAYSCALE:
                out_size = out_width * out_height;
                break;
            case InputImageLayerDesc::ColorSpace::YUV:
                out_size = 3 * out_width * out_height / 2;
                break;
            default:
                break;
            }
        } else {
            // TODO: Is it possible to have pipelines like this:
            // <video> ! <preproc> ! <inference> ! <postproc> ! <preproc> ! <inference> ! ...
            // In that case we won't have GstVideoInfo in the second preproc. Need to get input tensor caps instead

            // TODO: does input size always correlate with width/height of the video frame?
            out_size =
                (size * out_width * out_height) / (self->props.input_info->width * self->props.input_info->height);
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to calculate output buffer size: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    *othersize = out_size;

    return true;
}

static void gst_gva_video_to_tensor_class_init(GstGvaVideoToTensorClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_video_to_tensor_set_property;
    gobject_class->get_property = gst_gva_video_to_tensor_get_property;
    gobject_class->dispose = gst_gva_video_to_tensor_dispose;
    gobject_class->finalize = gst_gva_video_to_tensor_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gst_gva_video_to_tensor_set_caps;
    base_transform_class->sink_event = gst_gva_video_to_tensor_sink_event;
    base_transform_class->transform_caps = gst_gva_video_to_tensor_transform_caps;
    base_transform_class->start = gst_gva_video_to_tensor_start;
    base_transform_class->stop = gst_gva_video_to_tensor_stop;
    base_transform_class->transform_size = gst_gva_video_to_tensor_transform_size;
    base_transform_class->transform = gst_gva_video_to_tensor_transform;
    base_transform_class->transform_ip = gst_gva_video_to_tensor_transform_ip;
    base_transform_class->transform_ip_on_passthrough = FALSE;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_VIDEO_TO_TENSOR_NAME, "application",
                                          GVA_VIDEO_TO_TENSOR_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_OUTPUT_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_VIDEO_CAPS)));

    g_object_class_install_property(gobject_class, PROP_MODEL_PROC,
                                    g_param_spec_string("model-proc", "Model proc", "Path to model proc file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_PRE_PROC_BACKEND,
                                    g_param_spec_enum("pre-proc-backend", "Preproc backend",
                                                      "Preprocessing backend type",
                                                      GST_TYPE_GVA_VIDEO_TO_TENSOR_BACKEND, DEFAULT_PRE_PROC_BACKEND,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
