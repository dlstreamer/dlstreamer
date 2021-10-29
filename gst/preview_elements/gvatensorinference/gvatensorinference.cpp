/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensorinference.hpp"

#include <capabilities/tensor_caps.hpp>
#include <capabilities/types.hpp>
#include <frame_data.hpp>
#include <gva_utils.h>
#include <memory_type.hpp>
#include <meta/gva_buffer_flags.hpp>
#include <meta/gva_roi_ref_meta.hpp>
#include <safe_arithmetic.hpp>
#include <scope_guard.h>
#include <tensor_layer_desc.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

GST_DEBUG_CATEGORY(gva_tensor_inference_debug_category);

G_DEFINE_TYPE(GvaTensorInference, gva_tensor_inference, GST_TYPE_BASE_TRANSFORM);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_DEVICE,
    PROP_INFERENCE_INTERVAL, /* TODO: enable if needed */
    PROP_RESHAPE,            /* TODO: enable if needed */
    PROP_BATCH_SIZE,
    PROP_RESHAPE_WIDTH,  /* TODO: enable if needed */
    PROP_RESHAPE_HEIGHT, /* TODO: enable if needed */
    PROP_NO_BLOCK,       /* TODO: enable if needed */
    PROP_NIREQ,
    PROP_INSTANCE_ID,            /* TODO: enable if needed */
    PROP_PRE_PROC_BACKEND,       /* TODO: enable if needed */
    PROP_MODEL_PROC,             /* TODO: enable if needed */
    PROP_CPU_THROUGHPUT_STREAMS, /* TODO: enable if needed */
    PROP_GPU_THROUGHPUT_STREAMS, /* TODO: enable if needed */
    PROP_IE_CONFIG,
    PROP_DEVICE_EXTENSIONS, /* TODO: enable if needed */
    PROP_INFERENCE_REGION,  /* TODO: enable if needed */
    PROP_OBJECT_CLASS       /* TODO: should be out of scope of this element */
};

namespace {

constexpr guint DEFAULT_MIN_NIREQ = 0;
constexpr guint DEFAULT_MAX_NIREQ = 1024;
constexpr guint DEFAULT_NIREQ = DEFAULT_MIN_NIREQ;
constexpr guint DEFAULT_MIN_BATCH_SIZE = 1;
constexpr guint DEFAULT_MAX_BATCH_SIZE = 1024;
constexpr guint DEFAULT_BATCH_SIZE = DEFAULT_MIN_BATCH_SIZE;
constexpr auto DEFAULT_DEVICE = "CPU";

bool gva_tensor_inference_stopped(GvaTensorInference *self) {
    GstState state;
    bool is_stopped;

    GST_OBJECT_LOCK(self);
    state = GST_STATE(self);
    is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
    GST_OBJECT_UNLOCK(self);
    return is_stopped;
}

bool ensure_ie(GvaTensorInference *self) {
    if (self->props.infer) {
        return true;
    }

    if (self->props.model.empty()) {
        GST_ERROR_OBJECT(self, "Couldn't create IE: model path not set!");
        return false;
    }

    try {
        GST_INFO_OBJECT(self, "Creating IE...");
        self->props.infer = InferenceInstances::get(self->props.instance_id, self->props.model);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Couldn't create IE: %s", Utils::createNestedErrorMsg(e).c_str());
    }

    return self->props.infer != nullptr;
}

std::deque<GstVideoRegionOfInterestMeta *> GetFilteredRoisMeta(GstBuffer *buf,
                                                               const std::vector<std::string> &cls_filters) {

    std::deque<GstVideoRegionOfInterestMeta *> result;

    GstVideoRegionOfInterestMeta *meta = nullptr;
    gpointer state = nullptr;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, &state))) {
        if (!cls_filters.empty()) {
            auto compare_quark_string = [meta](const std::string &str) -> bool {
                const gchar *roi_type = meta->roi_type ? g_quark_to_string(meta->roi_type) : "";
                return (strcmp(roi_type, str.c_str()) == 0);
            };
            const bool found =
                std::find_if(cls_filters.cbegin(), cls_filters.cend(), compare_quark_string) != cls_filters.cend();
            if (!found)
                continue;
        }
        result.emplace_back(meta);
    }

    return result;
}

std::tuple<TensorInference::PreProcInfo, TensorInference::ImageInfo, GstVideoInfo *>
get_preproc_info(GvaTensorInference *self) {
    /* get event */
    GstEvent *event = gst_pad_get_sticky_event(GST_BASE_TRANSFORM(self)->sinkpad,
                                               static_cast<GstEventType>(GvaEventTypes::GVA_EVENT_PREPROC_INFO), 0);
    if (!event)
        return {};
    auto event_sg = makeScopeGuard([event] { gst_event_unref(event); });
    /* get & check structure from event */
    const GstStructure *structure = gst_event_get_structure(event);
    const gchar *name = gst_structure_get_name(structure);
    if (!g_str_has_prefix(name, "pre-proc-info"))
        return {};
    if (gst_structure_n_fields(structure) == 0) {
        /* pre-proc is not needed */
        return {};
    }
    /* get video info */
    TensorInference::ImageInfo image;
    // TODO: Do we need to free video_info?
    GstVideoInfo *video_info = nullptr;
    if (gst_structure_get(structure, "video-info", GST_TYPE_VIDEO_INFO, &video_info, nullptr) && video_info) {
        image.channels = get_channels_count(GST_VIDEO_INFO_FORMAT(video_info));
        image.width = safe_convert<uint32_t>(GST_VIDEO_INFO_WIDTH(video_info));
        image.height = safe_convert<uint32_t>(GST_VIDEO_INFO_HEIGHT(video_info));
        image.memory_type = self->props.input_caps.GetMemoryType();
    }
    /* get preproc info*/
    TensorInference::PreProcInfo preproc;
    auto ret = gst_structure_get(structure, "resize-algo", G_TYPE_INT, &preproc.resize_alg, "color-format", G_TYPE_UINT,
                                 &preproc.color_format, "va-display", G_TYPE_POINTER, &preproc.va_display, nullptr);
    if (!ret)
        return {};

    return std::make_tuple(preproc, image, video_info);
}

} /* anonymous namespace */

void GvaTensorInference::RunInference(GstBuffer *inbuf, GstBuffer *outbuf) {
    auto input = std::make_shared<FrameData>();
    if (!input)
        throw std::runtime_error("Failed to allocate input FrameData");

    // TODO: need to implement some custom TensorDescMeta to describe what type of data is inside (e.g. image, audio,
    // raw) and always attach it to tensor buffer so that we could map data basing on it

    // TODO: Support multiple model inputs
    auto first_input_tensor_caps = props.input_caps.GetTensorDesc(0);
    if (props.input_video_info) {
        input->Map(inbuf, props.input_video_info, first_input_tensor_caps.GetMemoryType(), GST_MAP_READ);
    } else {
        // TODO: provide correct plane sizes based on format
        input->Map(inbuf, first_input_tensor_caps, GST_MAP_READ, first_input_tensor_caps.GetMemoryType());
    }

    const auto &output_sizes = props.infer->GetTensorOutputSizes();
    auto model_name = props.infer->GetModelName();
    auto itt_name = std::string(GST_ELEMENT_NAME(this)) + " " + "Inference Completion Callback";

    auto submit_to_inference = [&](GstBuffer *out_buffer, GstVideoRegionOfInterestMeta *roi_meta) {
        auto output = std::make_shared<FrameData>();
        {
            ITT_TASK("ALLOC MEMORY AND MAP");
            /* memory of IE to put output blob in */
            auto mem = props.infer_pool->acquire();
            auto size = props.infer_pool->chunk_size();
            using memory_type = typename MemoryPool::pointer;
            /* wrapping acquired memory in smart type to set a deleter which will return it into the pool */
            auto wrapper = new SmartWrapper<memory_type>(mem, [&](memory_type mem) { props.infer_pool->release(mem); });
            auto on_delete = [](gpointer user_data) { delete static_cast<SmartWrapper<memory_type> *>(user_data); };
            auto gst_mem = gst_memory_new_wrapped((GstMemoryFlags)0, mem, size, 0, size, wrapper, on_delete);
            gst_buffer_append_memory(out_buffer, gst_mem);

            // TODO: add multiple TensorDesc processing
            output->Map(out_buffer, props.output_caps.GetTensorDesc(0), GST_MAP_WRITE,
                        InferenceBackend::MemoryType::SYSTEM, output_sizes.size(), output_sizes);
        }

        /* declare mutable as we need to capture non-const variables */
        auto completion_callback = [itt_name, this, input, output, inbuf,
                                    out_buffer](const std::string &error_msg) mutable {
            std::list<GstBuffer *> queue;
            {
                ITT_TASK(itt_name);

                output.reset();
                auto unique = input.use_count() == 1;
                input.reset();
                if (unique)
                    gst_buffer_unref(inbuf);

                /* handling inference errors */
                /* TODO: create more appropriate check
                 * TODO: copy and refactor logger from inference backend
                 * TODO: do we need to push output with invalid data?
                 */
                if (!error_msg.empty())
                    GST_WARNING_OBJECT(this, "Inference Error: %s", error_msg.c_str());

                /* check if pipeline is still running */
                if (gva_tensor_inference_stopped(this)) {
                    return;
                }

                /* handle internal queue */
                gst_buffer_set_flags(out_buffer, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH));
                queue = props.infer_queue.shrink([&](GstBuffer *buf) -> bool {
                    return gst_buffer_has_flags(buf, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH)) ||
                           gst_buffer_has_flags(buf, GST_BUFFER_FLAG_GAP);
                });
            }

            ITT_TASK("PUSH");
            for (const auto &e : queue) {
                if (gst_buffer_has_flags(e, GST_BUFFER_FLAG_GAP)) {
                    auto gap_event = gst_event_new_gap(GST_BUFFER_TIMESTAMP(e), GST_BUFFER_TIMESTAMP(e));
                    gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(&this->base), gap_event);
                    gst_buffer_unref(e);
                } else {
                    gst_buffer_unset_flags(e, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH));
                    gst_pad_push(this->base.srcpad, e);
                }
            }
        };

        TensorInference::RoiRect roi;
        if (roi_meta) {
            auto roi_ref_meta = GVA_ROI_REF_META_ADD(out_buffer);
            roi_ref_meta->reference_roi_id = roi_meta->id;

            roi.x = roi_meta->x;
            roi.y = roi_meta->y;
            roi.w = roi_meta->w;
            roi.h = roi_meta->h;
        }

        ITT_TASK("INFER ASYNC");
        props.infer->InferAsync(input, output, completion_callback, roi);
    };

    // Collect and filter ROI
    auto rois_meta = GetFilteredRoisMeta(inbuf, props.obj_classes_filter);

    if (!rois_meta.empty()) {
        std::vector<GstBuffer *> roi_bufs;
        roi_bufs.reserve(rois_meta.size());
        roi_bufs.push_back(outbuf);
        for (auto i = 0u; i < rois_meta.size() - 1; i++) {
            auto roi_buf = gst_buffer_new();
            gst_buffer_copy_into(roi_buf, outbuf, GST_BUFFER_COPY_METADATA, 0, -1);
            roi_bufs.push_back(roi_buf);
            props.infer_queue.push(roi_buf);
        }

#ifdef MICRO_ROI_NO_SPLIT
        // Mark last ROI on frame
        gst_buffer_set_flags(roi_bufs.back(), (GstBufferFlags)GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME);
#endif

        // Submit inference
        std::unique_lock<TensorInference> lock(*props.infer);
        for (size_t i = 0; i < roi_bufs.size(); i++) {
            auto meta = rois_meta.at(i);
            auto buf = roi_bufs.at(i);
            submit_to_inference(buf, meta);
        }
    } else {
        submit_to_inference(outbuf, nullptr);
        // TODO: hint for muxer to not wait more data for current buffer
        gst_buffer_set_flags(outbuf, (GstBufferFlags)GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME);
    }
}

static void gva_tensor_inference_init(GvaTensorInference *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (!self)
        return;

    // Initialize C++ structure with new
    new (&self->props) GvaTensorInference::_Props();

    self->props.nireq = DEFAULT_NIREQ;
    self->props.batch_size = DEFAULT_BATCH_SIZE;
    self->props.device = DEFAULT_DEVICE;
}

static void gva_tensor_inference_set_property(GObject *object, guint property_id, const GValue *value,
                                              GParamSpec *pspec) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (property_id) {
    case PROP_MODEL:
        self->props.model = g_value_get_string(value);
        break;
    case PROP_IE_CONFIG:
        self->props.ie_config = g_value_get_string(value);
        break;
    case PROP_NIREQ:
        self->props.nireq = g_value_get_uint(value);
        break;
    case PROP_BATCH_SIZE:
        self->props.batch_size = g_value_get_uint(value);
        if (self->props.batch_size != 1) {
            GST_ERROR_OBJECT(self, "Batch-size can only be equal to 1 at the moment.");
            throw std::logic_error("Not implemented yet.");
        }
        break;
    case PROP_INSTANCE_ID:
        self->props.instance_id = g_value_get_string(value);
        break;
    case PROP_DEVICE:
        self->props.device = g_value_get_string(value);
        break;
    case PROP_OBJECT_CLASS:
        self->props.object_class = g_value_get_string(value);
        self->props.obj_classes_filter = Utils::splitString(self->props.object_class, ',');
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
        break;
    }
}

static void gva_tensor_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_string(value, self->props.model.c_str());
        break;
    case PROP_IE_CONFIG:
        g_value_set_string(value, self->props.ie_config.c_str());
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, self->props.nireq);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, self->props.batch_size);
        break;
    case PROP_INSTANCE_ID:
        g_value_set_string(value, self->props.instance_id.c_str());
        break;
    case PROP_DEVICE:
        g_value_set_string(value, self->props.device.c_str());
        break;
    case PROP_OBJECT_CLASS:
        g_value_set_string(value, self->props.object_class.c_str());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
        break;
    }
}

static void gva_tensor_inference_dispose(GObject *object) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gva_tensor_inference_parent_class)->dispose(object);
}

static void gva_tensor_inference_finalize(GObject *object) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    gst_video_info_free(self->props.input_video_info);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_tensor_inference_parent_class)->finalize(object);
}

static GstCaps *gva_tensor_inference_transform_caps(GstBaseTransform *base, GstPadDirection direction,
                                                    GstCaps * /*caps*/, GstCaps *filter) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* acquiring object name here because it is not available in init */
    if (self->props.instance_id.empty()) {
        self->props.instance_id = self->base.element.object.name; // TODO: add uid or smth similar
    }

    // Ensure that we have IE and model is loaded
    if (!ensure_ie(self)) {
        GST_ELEMENT_ERROR(self, LIBRARY, INIT, ("Couldn't init Inference Engine"), ("empty inference instance"));
        return gst_caps_new_empty();
    }
    g_assert(self->props.infer);

    // TODO: move caps building in some common place (TensorCaps?)
    GstCaps *result_caps = nullptr;

    switch (direction) {
    case GST_PAD_SRC: {
        result_caps = gst_caps_from_string(GVA_TENSOR_CAPS GVA_VAAPI_TENSOR_CAPS GVA_TENSORS_CAPS);
        break;
    }
    case GST_PAD_SINK: {
        auto mem_type = InferenceBackend::MemoryType::SYSTEM;
        std::vector<TensorCaps> tensor_caps;
        for (const auto &desc : self->props.infer->GetTensorOutputInfo()) {
            // TODO: can we use everywhere IE precision and layout types ?
            tensor_caps.emplace_back(mem_type, static_cast<Precision>(static_cast<int>(desc.precision)),
                                     static_cast<Layout>(static_cast<int>(desc.layout)), desc.dims, desc.layer_name);
        }
        result_caps = TensorCapsArray::ToCaps(TensorCapsArray(tensor_caps));
        break;
    }
    default:
        GST_WARNING_OBJECT(self, "Unknown pad direction in _transform_caps");
        return nullptr;
    }

    if (filter) {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(base, "Using filter caps %" GST_PTR_FORMAT, filter);

        intersection = gst_caps_intersect_full(filter, result_caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(result_caps);
        result_caps = intersection;

        GST_DEBUG_OBJECT(base, "Intersection %" GST_PTR_FORMAT, result_caps);
    }

    return result_caps;
}

static gboolean gva_tensor_inference_transform_size(GstBaseTransform *base, GstPadDirection direction,
                                                    GstCaps * /*caps*/, gsize /*size*/, GstCaps * /*othercaps*/,
                                                    gsize *othersize) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* GStreamer hardcoded call with GST_PAD_SINK only */
    g_assert(direction == GST_PAD_SINK);
    *othersize = 0;

    return true;
}

static gboolean gva_tensor_inference_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* acquiring object name here because it is not available in init */
    if (self->props.instance_id.empty()) {
        self->props.instance_id = self->base.element.object.name;
    }

    try {
        self->props.input_caps = TensorCapsArray::FromCaps(incaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse input caps: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        self->props.output_caps = TensorCapsArray::FromCaps(outcaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse output caps: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        ITT_TASK("INIT IE");
        TensorInference::PreProcInfo preproc;
        TensorInference::ImageInfo image;
        GstVideoInfo *video_info = nullptr;
        std::tie(preproc, image, video_info) = get_preproc_info(self);

        if (video_info)
            self->props.input_video_info = gst_video_info_copy(video_info);

        self->props.infer->Init(self->props.device, self->props.nireq, self->props.ie_config, preproc, image);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to initialize TensorInference: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    /* memory pool */
    try {
        GST_INFO_OBJECT(self, "Creating Memory Pool...");
        auto output_sizes = self->props.infer->GetTensorOutputSizes();
        auto sum = std::accumulate(output_sizes.begin(), output_sizes.end(), 0lu);
        self->props.infer_pool = std::make_shared<MemoryPool>(sum, self->props.infer->GetRequestsNum());
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Couldn't create Memory Pool: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    return true;
}

static gboolean gva_tensor_inference_sink_event(GstBaseTransform *base, GstEvent *event) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (event->type) {
    case GST_EVENT_EOS:
        if (self->props.infer)
            self->props.infer->Flush();
        break;
    case GST_EVENT_GAP:
        if (!self->props.infer_queue.empty()) {
            // Need to manually handle gap event here because of asynchronous behaviour
            // Create buffer from gap event and push to inference queue
            // We will send gap_event again for this buffer in inference completion callback
            GstClockTime pts;
            GstClockTime duration;
            gst_event_parse_gap(event, &pts, &duration);
            auto gapbuf = gst_buffer_new();
            GST_BUFFER_PTS(gapbuf) = pts;
            GST_BUFFER_DURATION(gapbuf) = duration;
            GST_BUFFER_FLAG_SET(gapbuf, GST_BUFFER_FLAG_GAP);
            self->props.infer_queue.push(gapbuf);
            return true;
        }
        break;
    default:
        break;
    }

    /* do not need to forward this event because preproc information
     * is only for the first inference element to use */
    if (event->type == static_cast<GstEventType>(GvaEventTypes::GVA_EVENT_PREPROC_INFO))
        return true;

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_inference_parent_class)->sink_event(base, event);
}

static gboolean gva_tensor_inference_start(GstBaseTransform *base) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self,
                    "%s parameters:\n -- Model: %s\n -- IE config: %s\n -- Device: %s\n "
                    "-- Num of reqests: %d\n -- Batch size: %d\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)), self->props.model.c_str(), self->props.ie_config.c_str(),
                    self->props.device.c_str(), self->props.nireq, self->props.batch_size);

    return true;
}

static gboolean gva_tensor_inference_stop(GstBaseTransform *base) {
    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static GstFlowReturn gva_tensor_inference_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    ITT_TASK(std::string(GST_ELEMENT_NAME(base)) + " " + __FUNCTION__);

    GvaTensorInference *self = GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // No memory is OK for vaapi
    g_assert(gst_buffer_n_memory(inbuf) <= 1);
    g_assert(gst_buffer_n_memory(outbuf) == 0);
    g_assert(self->props.infer);
#ifndef MICRO_ROI_NO_SPLIT
    g_assert(gst_buffer_get_n_meta(inbuf, GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE) <= 1);
#endif

    gst_buffer_ref(inbuf);
    /* We need to copy outbuf, otherwise we might have buffer with ref > 1 on pad_push */
    /* To set buffer for IE to write output blobs we need a writable memory in outbuf (if we just copy refcount of
     * memory will increase) */
    auto copy = gst_buffer_copy(outbuf);
    /* queueing the buffer for synchronization */
    self->props.infer_queue.push(copy);
    try {
        ITT_TASK("START INFERENCE");
        self->RunInference(inbuf, copy);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during inference: %s", Utils::createNestedErrorMsg(e).c_str());
        gst_buffer_unref(inbuf);
        gst_buffer_unref(copy);
        return GST_FLOW_ERROR;
    }

    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static gboolean gva_tensor_inference_query(GstBaseTransform *base, GstPadDirection direction, GstQuery *query) {
    auto self = GVA_TENSOR_INFERENCE(base);
    switch (direction) {
    case GST_PAD_SINK: {
        if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_INPUT)) {
            if (!self->props.infer)
                return false;

            auto structure = gst_query_writable_structure(query);
            const auto &inputs = self->props.infer->GetTensorInputInfo();
            auto inputs_array = g_array_new(false, false, sizeof(TensorLayerDesc));
            for (const auto &desc : inputs)
                g_array_append_val(inputs_array, desc);
            gst_structure_set(structure, "inputs", G_TYPE_ARRAY, inputs_array, nullptr);
            return true;
        }
        break;
    }
    case GST_PAD_SRC: {
        if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_OUTPUT)) {
            if (!self->props.infer)
                return false;

            auto structure = gst_query_writable_structure(query);
            const auto &outputs = self->props.infer->GetTensorOutputInfo();
            auto outputs_array = g_array_new(false, false, sizeof(TensorLayerDesc));
            for (const auto &desc : outputs)
                g_array_append_val(outputs_array, desc);
            gst_structure_set(structure, "outputs", G_TYPE_ARRAY, outputs_array, nullptr);
            return true;
        }
    }
    default:
        break;
    }

    if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_INFO)) {
        if (!self->props.infer)
            return false;

        auto structure = gst_query_writable_structure(query);
        auto model_name = self->props.infer->GetModelName();
        gst_structure_set(structure, "model_name", G_TYPE_STRING, model_name.c_str(), "instance_id", G_TYPE_STRING,
                          self->props.instance_id.c_str(), nullptr);
        return true;
    }

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_inference_parent_class)->query(base, direction, query);
}

static void gva_tensor_inference_class_init(GvaTensorInferenceClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_tensor_inference_set_property;
    gobject_class->get_property = gva_tensor_inference_get_property;
    gobject_class->dispose = gva_tensor_inference_dispose;
    gobject_class->finalize = gva_tensor_inference_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gva_tensor_inference_set_caps;
    base_transform_class->sink_event = gva_tensor_inference_sink_event;
    base_transform_class->transform_caps = gva_tensor_inference_transform_caps;
    base_transform_class->transform_size = gva_tensor_inference_transform_size;
    base_transform_class->start = gva_tensor_inference_start;
    base_transform_class->stop = gva_tensor_inference_stop;
    base_transform_class->transform = gva_tensor_inference_transform;
    base_transform_class->query = gva_tensor_inference_query;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_INFERENCE_NAME, "application",
                                          GVA_TENSOR_INFERENCE_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
    gst_element_class_add_pad_template(
        element_class,
        gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                             gst_caps_from_string(GVA_TENSOR_CAPS GVA_VAAPI_TENSOR_CAPS GVA_TENSORS_CAPS)));

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model", "Path to inference model network file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobject_class, PROP_DEVICE,
                                    g_param_spec_string("device", "Device", "Inference device: [CPU, GPU]",
                                                        DEFAULT_DEVICE,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests",
                                                      DEFAULT_MIN_NIREQ, DEFAULT_MAX_NIREQ, DEFAULT_NIREQ,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_BATCH_SIZE,
        g_param_spec_uint("batch-size", "Batch Size",
                          "Number of frames batched together for a single inference. Not all models support batching. "
                          "Use model optimizer to ensure that the model has batching support.",
                          DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property( // TODO: verify description
        gobject_class, PROP_INSTANCE_ID,
        g_param_spec_string(
            "instance-id", "Instance ID",
            "Identifier for sharing resources between inference elements of the same type. Elements with "
            "the instance-id will share model and other properties",
            NULL, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", NULL,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "ObjectClass",
                            "Filter for Region of Interest class label on this element input", "",
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
