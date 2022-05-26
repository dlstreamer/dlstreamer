/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensorinference.hpp"

#include <capabilities/tensor_caps.hpp>
#include <capabilities/types.hpp>
#include <frame_data.hpp>
#include <gva_roi_ref_meta.hpp>
#include <gva_utils.h>
#include <memory_type.hpp>
#include <meta/gva_buffer_flags.hpp>
#include <safe_arithmetic.hpp>
#include <scope_guard.h>
#include <tensor_layer_desc.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

GST_DEBUG_CATEGORY(gva_tensor_inference_debug_category);
#define GST_CAT_DEFAULT gva_tensor_inference_debug_category

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

std::tuple<TensorInference::PreProcInfo, TensorInference::ImageInfo, GstVideoInfo *>
get_preproc_info(GvaTensorInference *self) {
    /* get event */
    GstEvent *event = gst_pad_get_sticky_event(GST_BASE_TRANSFORM(self)->sinkpad,
                                               static_cast<GstEventType>(GvaEventTypes::GVA_EVENT_PREPROC_INFO), 0);
    if (!event)
        return {};

    auto event_guard = makeScopeGuard([event]() { gst_event_unref(event); });

    GstVideoInfo *video_info = nullptr;
    TensorInference::PreProcInfo preproc;
    int32_t resize_alg;
    uint32_t color_format;
    if (!gva_event_parse_preproc_info(event, video_info, resize_alg, color_format, preproc.va_display))
        return {};

    preproc.color_format = static_cast<InferenceEngine::ColorFormat>(color_format);
    preproc.resize_alg = static_cast<InferenceEngine::ResizeAlgorithm>(resize_alg);

    TensorInference::ImageInfo image;
    if (video_info) {
        image.channels = get_channels_count(GST_VIDEO_INFO_FORMAT(video_info));
        image.width = safe_convert<uint32_t>(GST_VIDEO_INFO_WIDTH(video_info));
        image.height = safe_convert<uint32_t>(GST_VIDEO_INFO_HEIGHT(video_info));
        image.memory_type = self->props.input_caps.GetMemoryType();
    }

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
        gst_buffer_append_memory(outbuf, gst_mem);

        // TODO: add multiple TensorDesc processing
        output->Map(outbuf, props.output_caps.GetTensorDesc(0), GST_MAP_WRITE, InferenceBackend::MemoryType::SYSTEM,
                    output_sizes.size(), output_sizes);
    }

    /* declare mutable as we need to capture non-const variables */
    auto completion_callback = [itt_name, this, input, output, inbuf, outbuf](const std::string &error_msg) mutable {
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
            gst_buffer_set_flags(outbuf, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH));
            std::lock_guard<decltype(props.infer_queue)> queue_lock(props.infer_queue);
            queue = props.infer_queue.shrink([&](GstBuffer *buf) -> bool {
                return gst_buffer_has_flags(buf, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH)) ||
                       gst_buffer_has_flags(buf, GST_BUFFER_FLAG_GAP);
            });

            for (const auto &e : queue) {
                if (gst_buffer_has_flags(e, GST_BUFFER_FLAG_GAP)) {
                    // TODO: if buffer has DROPPABLE flag than it's original GST_GAP_EVENT
                    GstEvent *gap_event = nullptr;
                    gst_buffer_unset_flags(e, GST_BUFFER_FLAG_GAP);
                    if (gst_buffer_has_flags(e, GST_BUFFER_FLAG_DROPPABLE))
                        gap_event = gst_event_new_gap(GST_BUFFER_PTS(e), GST_BUFFER_DURATION(e));
                    else
                        gap_event = gva_event_new_gap_with_buffer(e);

                    GST_DEBUG_OBJECT(this, "GAP buffer from queue. Propagate GAP event: ts=%" GST_TIME_FORMAT,
                                     GST_TIME_ARGS(GST_BUFFER_PTS(e)));
                    gst_buffer_unref(e);
                    gst_pad_push_event(base.srcpad, gap_event);
                } else {
                    gst_buffer_unset_flags(e, static_cast<GstBufferFlags>(GST_BUFFER_FLAG_READY_TO_PUSH));

                    GST_DEBUG_OBJECT(this, "Push buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(e)));
                    gst_pad_push(this->base.srcpad, e);
                }
            }
        }
    };

    TensorInference::RoiRect roi;
    auto crop = gst_buffer_get_video_crop_meta(inbuf);
    if (crop) {
        roi.x = crop->x;
        roi.y = crop->y;
        roi.w = crop->width;
        roi.h = crop->height;
    }

    ITT_TASK("INFER ASYNC");
    props.infer->InferAsync(input, output, completion_callback, roi);
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

    GstCaps *result_caps = nullptr;

    switch (direction) {
    case GST_PAD_SRC: {
        result_caps = gst_caps_from_string(GVA_TENSORS_CAPS GVA_VAAPI_TENSORS_CAPS);
        break;
    }
    case GST_PAD_SINK: {
        auto mem_type = InferenceBackend::MemoryType::SYSTEM;
        std::vector<TensorCaps> tensor_caps;
        try {
            const auto &output_info = self->props.infer->GetTensorOutputInfo();
            std::transform(output_info.begin(), output_info.end(), std::back_inserter(tensor_caps),
                           [mem_type](const TensorLayerDesc &desc) {
                               return TensorCaps(mem_type, desc.precision, desc.layout, desc.dims);
                           });
            result_caps = TensorCapsArray::ToCaps(TensorCapsArray(tensor_caps));
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(self, "Error during transform caps: %s", Utils::createNestedErrorMsg(e).c_str());
            return nullptr;
        }
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
            self->props.input_video_info = video_info;

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
    case GST_EVENT_GAP: {
        std::lock_guard<decltype(self->props.infer_queue)> queue_lock(self->props.infer_queue);
        if (!self->props.infer_queue.empty()) {
            // Need to manually handle gap event here because of asynchronous behaviour
            // Create buffer from gap event and push to inference queue
            // We will send gap_event again for this buffer in inference completion callback
            GstBuffer *gapbuf = nullptr;
            if (!gva_event_parse_gap_with_buffer(event, &gapbuf) || !gapbuf) {
                gapbuf = gst_buffer_new();
                gst_event_parse_gap(event, &GST_BUFFER_PTS(gapbuf), &GST_BUFFER_DURATION(gapbuf));
                gst_buffer_set_flags(gapbuf, GST_BUFFER_FLAG_DROPPABLE);
            }
            GST_DEBUG_OBJECT(self, "Queued GAP buffer from event: ts=%" GST_TIME_FORMAT,
                             GST_TIME_ARGS(GST_BUFFER_PTS(gapbuf)));
            gst_buffer_set_flags(gapbuf, GST_BUFFER_FLAG_GAP);
            gst_event_unref(event);
            self->props.infer_queue.push(gapbuf);
            return true;
        }
        break;
    }
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
    g_assert(gst_buffer_get_n_meta(inbuf, GST_VIDEO_CROP_META_API_TYPE) <= 1);
    g_assert(self->props.infer);

    gst_buffer_ref(inbuf);
    /* We need to copy outbuf, otherwise we might have buffer with ref > 1 on pad_push */
    /* To set buffer for IE to write output blobs we need a writable memory in outbuf (if we just copy refcount of
     * memory will increase) */
    auto copy = gst_buffer_copy(outbuf);
    /* queueing the buffer for synchronization */
    {
        std::lock_guard<decltype(self->props.infer_queue)> queue_lock(self->props.infer_queue);
        self->props.infer_queue.push(copy);
    }

    // Copy flags in case if there is a LAST_FRAME_ON_ROI flag
    gst_buffer_copy_into(copy, inbuf, GST_BUFFER_COPY_FLAGS, 0, static_cast<gsize>(-1));

    try {
        GST_DEBUG_OBJECT(self, "Transform buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(inbuf)));
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
    case GST_PAD_SRC: {
        if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_OUTPUT)) {
            if (!self->props.infer)
                return false;

            if (!gva_query_fill_model_output(query, self->props.infer->GetTensorOutputInfo()))
                GST_ERROR_OBJECT(self, "Failed to fill model output query");

            return true;
        }
    }
        /* FALLTHRU */
    case GST_PAD_SINK: {
        if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_INPUT)) {
            if (!self->props.infer)
                return false;
            if (!gva_query_fill_model_input(query, self->props.infer->GetTensorInputInfo().front()))
                GST_ERROR_OBJECT(self, "Failed to fill model input query");

            return true;
        }
        break;
    }
    default:
        break;
    }

    if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_INFO)) {
        if (!self->props.infer)
            return false;

        if (!gva_query_fill_model_info(query, self->props.infer->GetModelName(), self->props.instance_id))
            GST_ERROR_OBJECT(self, "Failed to fill model info query");

        return true;
    }

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_inference_parent_class)->query(base, direction, query);
}

static gboolean gva_tensor_inference_transform_meta(GstBaseTransform *base, GstBuffer *outbuf, GstMeta *meta,
                                                    GstBuffer *inbuf) {
    if (meta->info->api == gva_roi_ref_meta_api_get_type())
        return true;

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_inference_parent_class)->transform_meta(base, outbuf, meta, inbuf);
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
    base_transform_class->transform_meta = gva_tensor_inference_transform_meta;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_INFERENCE_NAME, "application",
                                          GVA_TENSOR_INFERENCE_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                            gst_caps_from_string(GVA_TENSORS_CAPS GVA_VAAPI_TENSORS_CAPS)));

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
}

static gboolean plugin_init(GstPlugin *plugin) {
    GST_DEBUG_CATEGORY_INIT(gva_tensor_inference_debug_category, "gvatensorinference_debug", 0,
                            "Debug category of gvatensorinference");

    return gst_element_register(plugin, "gvatensorinference", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_INFERENCE);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_openvino,
                  PRODUCT_FULL_NAME " OpenVINO™ Toolkit inference element", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
