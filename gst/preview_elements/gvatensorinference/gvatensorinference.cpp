/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensorinference.hpp"

#include <capabilities/capabilities.hpp>
#include <capabilities/types.hpp>
#include <frame_data.hpp>
#include <gva_custom_meta.hpp>
#include <memory_type.hpp>
#include <utils.h>

GST_DEBUG_CATEGORY(gst_gva_tensor_inference_debug_category);

G_DEFINE_TYPE(GstGvaTensorInference, gst_gva_tensor_inference, GST_TYPE_BASE_TRANSFORM);

static constexpr guint DEFAULT_MIN_NIREQ = 1;
static constexpr guint DEFAULT_MAX_NIREQ = 1024;
static constexpr guint DEFAULT_NIREQ = DEFAULT_MIN_NIREQ;

static constexpr guint DEFAULT_MIN_BATCH_SIZE = 1;
static constexpr guint DEFAULT_MAX_BATCH_SIZE = 1024;
static constexpr guint DEFAULT_BATCH_SIZE = DEFAULT_MIN_BATCH_SIZE;

static constexpr auto DEFAULT_DEVICE = "CPU";

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
    PROP_MODEL_INSTANCE_ID,      /* TODO: enable if needed */
    PROP_PRE_PROC_BACKEND,       /* TODO: enable if needed */
    PROP_MODEL_PROC,             /* TODO: enable if needed */
    PROP_CPU_THROUGHPUT_STREAMS, /* TODO: enable if needed */
    PROP_GPU_THROUGHPUT_STREAMS, /* TODO: enable if needed */
    PROP_IE_CONFIG,
    PROP_DEVICE_EXTENSIONS, /* TODO: enable if needed */
    PROP_INFERENCE_REGION,  /* TODO: enable if needed */
    PROP_OBJECT_CLASS       /* TODO: enable if needed */
};

namespace {

std::tuple<TensorInference::PreProcInfo, TensorInference::ImageInfo>
GetMetaInfo(GstBuffer *buffer, InferenceBackend::MemoryType mem_type) {
    TensorInference::PreProcInfo preproc;
    TensorInference::ImageInfo image;

    GstGVACustomMeta *meta = GST_GVA_CUSTOM_META_GET(buffer);
    if (meta) {
        preproc.resize_alg = meta->pre_process_info->getResizeAlgorithm();
        preproc.color_format = meta->pre_process_info->getColorFormat();
        image.channels = meta->channels;
        image.width = meta->width;
        image.height = meta->height;
        image.memory_type = mem_type;
        image.va_display = get_va_info_from_buffer(buffer).va_display;
    }

    return std::make_tuple(preproc, image);
}

void RemoveMeta(GstBuffer *buffer) {
    GstGVACustomMeta *meta = GST_GVA_CUSTOM_META_GET(buffer);
    if (meta) {
        if (!gst_buffer_remove_meta(buffer, GST_META_CAST(meta))) {
            throw std::runtime_error("Failed to remove GVACustomMeta from buffer");
        }
    }
}

} /* anonymous namespace */

static void gst_gva_tensor_inference_init(GstGvaTensorInference *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (!self)
        return;

    // Initialize C++ structure with new
    new (&self->props) GstGvaTensorInference::_Props();

    self->props.nireq = DEFAULT_NIREQ;
    self->props.batch_size = DEFAULT_BATCH_SIZE;
    self->props.device = DEFAULT_DEVICE;
}

static void gst_gva_tensor_inference_set_property(GObject *object, guint property_id, const GValue *value,
                                                  GParamSpec *pspec) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(object);
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
    case PROP_DEVICE:
        self->props.device = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
        break;
    }
}

static void gst_gva_tensor_inference_get_property(GObject *object, guint property_id, GValue *value,
                                                  GParamSpec *pspec) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(object);
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
    case PROP_DEVICE:
        g_value_set_string(value, self->props.device.c_str());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(self, property_id, pspec);
        break;
    }
}

static void gst_gva_tensor_inference_dispose(GObject *object) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gst_gva_tensor_inference_parent_class)->dispose(object);
}

static void gst_gva_tensor_inference_finalize(GObject *object) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gst_gva_tensor_inference_parent_class)->finalize(object);
}

static bool ensure_ie(GstGvaTensorInference *self) {
    if (self->props.infer) {
        return true;
    }

    if (self->props.model.empty()) {
        GST_ERROR_OBJECT(self, "Couldn't create IE: model path not set!");
        return false;
    }

    try {
        GST_INFO_OBJECT(self, "Creating IE...");
        self->props.infer = std::unique_ptr<TensorInference>(new TensorInference(self->props.model));
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Couldn't create IE: %s", Utils::createNestedErrorMsg(e).c_str());
    }

    return self->props.infer != nullptr;
}

gboolean gva_tensor_inference_stopped(GstGvaTensorInference *self) {
    GstState state;
    gboolean is_stopped;

    GST_OBJECT_LOCK(self);
    state = GST_STATE(self);
    is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
    GST_OBJECT_UNLOCK(self);
    return is_stopped;
}

GstCaps *gst_gva_tensor_inference_transform_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps * /*caps*/,
                                                 GstCaps *filter) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Ensure that we have IE and model is loaded
    if (!ensure_ie(self)) {
        return nullptr;
    }
    g_assert(self->props.infer);

    Precision precision = Precision::UNSPECIFIED;
    Layout layout = Layout::ANY;
    int channels = -1;
    int dim1 = -1;
    int dim2 = -1;

    switch (direction) {
    case GST_PAD_SRC: {
        auto input_info = self->props.infer->GetTensorInputInfo();
        precision = static_cast<Precision>(input_info.precision);
        layout = static_cast<Layout>(input_info.layout);
        channels = input_info.channels;
        dim1 = input_info.height;
        dim2 = input_info.width;
        break;
    }
    case GST_PAD_SINK: {
        auto output_info = self->props.infer->GetTensorOutputInfo();
        precision = static_cast<Precision>(output_info.precision);
        layout = static_cast<Layout>(output_info.layout);
        channels = output_info.channels;
        dim1 = output_info.height;
        dim2 = output_info.width;
        break;
    }
    default:
        GST_WARNING_OBJECT(self, "Unknown pad direction in _transform_caps");
        return nullptr;
    }

    std::string precision_str;
    std::string layout_str;
    if (!precision_to_string(precision, precision_str) || !layout_to_string(layout, layout_str)) {
        GST_ERROR_OBJECT(self, "Failed to construct capabilities: Unknown layout or precision");
        return nullptr;
    }

    // TODO: move caps building in some common place
    GstCaps *result = nullptr;
    switch (layout) {
    case Layout::NC:
        result = gst_caps_from_string(direction == GST_PAD_SRC ? GVA_TENSOR_CAPS_0 GVA_VAAPI_TENSOR_CAPS_0
                                                               : GVA_TENSOR_CAPS_0);
        break;
    case Layout::CHW:
        result = gst_caps_from_string(direction == GST_PAD_SRC ? GVA_TENSOR_CAPS_1 GVA_VAAPI_TENSOR_CAPS_1
                                                               : GVA_TENSOR_CAPS_1);
        break;
    case Layout::NCHW:
    case Layout::NHWC:
    default:
        result = gst_caps_from_string(direction == GST_PAD_SRC ? GVA_TENSOR_CAPS_2 GVA_VAAPI_TENSOR_CAPS_2
                                                               : GVA_TENSOR_CAPS_2);
        break;
    }

    gst_caps_set_simple(result, "precision", G_TYPE_STRING, precision_str.c_str(), "layout", G_TYPE_STRING,
                        layout_str.c_str(), nullptr);

    switch (layout) {
    case Layout::NCHW:
    case Layout::NHWC:
        gst_caps_set_simple(result, "batch-size", G_TYPE_INT, self->props.batch_size, nullptr);
        /* fall through*/
    case Layout::CHW:
        gst_caps_set_simple(result, "channels", G_TYPE_INT, channels, "dimension1", G_TYPE_INT, dim1, "dimension2",
                            G_TYPE_INT, dim2, nullptr);
        break;
    case Layout::NC:
        gst_caps_set_simple(result, "batch-size", G_TYPE_INT, self->props.batch_size, "channels", G_TYPE_INT, channels,
                            nullptr);
        break;
    default:
        GST_WARNING_OBJECT(self, "Unknown layout. Set all properties");
        gst_caps_set_simple(result, "batch-size", G_TYPE_INT, self->props.batch_size, "channels", G_TYPE_INT, channels,
                            "dimension1", G_TYPE_INT, dim1, "dimension2", G_TYPE_INT, dim2, nullptr);
        break;
    }

    if (filter) {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(base, "Using filter caps %" GST_PTR_FORMAT, filter);

        intersection = gst_caps_intersect_full(filter, result, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(result);
        result = intersection;

        GST_DEBUG_OBJECT(base, "Intersection %" GST_PTR_FORMAT, result);
    }

    return result;
}

gboolean gst_gva_tensor_inference_transform_size(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                                 gsize size, GstCaps *othercaps, gsize *othersize) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* GStreamer hardcoded call with GST_PAD_SINK only */
    g_assert(direction == GST_PAD_SINK);
    *othersize = 0;

    UNUSED(caps);
    UNUSED(size);
    UNUSED(othercaps);
    return true;
}

static gboolean gst_gva_tensor_inference_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    try {
        self->props.input_caps = TensorCaps(incaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse input caps: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        self->props.output_caps = TensorCaps(outcaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse output caps: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    return true;
}

static gboolean gst_gva_tensor_inference_sink_event(GstBaseTransform *base, GstEvent *event) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return GST_BASE_TRANSFORM_CLASS(gst_gva_tensor_inference_parent_class)->sink_event(base, event);
}

static gboolean gst_gva_tensor_inference_start(GstBaseTransform *base) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gst_gva_tensor_inference_stop(GstBaseTransform *base) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

void GstGvaTensorInference::RunInference(GstBuffer *inbuf, GstBuffer *outbuf) {
    auto input = std::make_shared<FrameData>();
    input->Map(inbuf, props.input_caps, GST_MAP_READ, 1, props.input_caps.GetMemoryType());

    /* memory of IE to put output blob in */
    auto out_blob_size = this->props.infer->GetTensorOutputInfo().size;
    auto data = new uint8_t[out_blob_size];
    auto on_delete = [](gpointer user_data) { delete[] reinterpret_cast<uint8_t *>(user_data); };
    auto out_mem = gst_memory_new_wrapped((GstMemoryFlags)0, data, out_blob_size, 0, out_blob_size, data, on_delete);
    gst_buffer_append_memory(outbuf, out_mem);

    auto output = std::make_shared<FrameData>();
    output->Map(outbuf, props.output_caps, GST_MAP_WRITE, 1, InferenceBackend::MemoryType::SYSTEM);

    /* declare mutable as we need to capture non-const variables */
    auto completion_callback = [this, input, output, inbuf, outbuf](const std::string &error_msg) mutable {
        input.reset();
        output.reset();
        gst_buffer_unref(inbuf);

        /* handling inference errors */
        /* TODO: create more appropriate check
         * TODO: copy and refactor logger from inference backend
         * TODO: do we need to push outpuf with invalid data?
         */
        if (!error_msg.empty())
            GST_WARNING_OBJECT(this, "Inference Error: %s", error_msg.c_str());

        /* check if pipeline is still running */
        if (gva_tensor_inference_stopped(this)) {
            return;
        }

        // TODO: we must have either an internal queue of queue after inference
        gst_pad_push(this->base.srcpad, outbuf);
    };

    props.infer->InferAsync(input, output, completion_callback);
}

GstFlowReturn gst_gva_tensor_inference_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GstGvaTensorInference *self = GST_GVA_TENSOR_INFERENCE(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // No memory is OK for vaapi
    g_assert(gst_buffer_n_memory(inbuf) <= 1);
    g_assert(gst_buffer_n_memory(outbuf) == 0);
    g_assert(self->props.infer);

    try {
        TensorInference::PreProcInfo preproc;
        TensorInference::ImageInfo image;
        std::tie(preproc, image) = GetMetaInfo(inbuf, self->props.input_caps.GetMemoryType());
        /* Lazy init to wait for meta */
        self->props.infer->Init(self->props.device, self->props.nireq, self->props.ie_config, preproc, image);
        // Needed only for the first inference
        RemoveMeta(inbuf);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to initialize TensorInference: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    gst_buffer_ref(inbuf);
    /* We need to copy outbuf, otherwise we might have buffer with ref > 1 on pad_push */
    /* To set buffer for IE to write output blobs we need a writable memory in outbuf (if we just copy refcount of
     * memory will increase) */
    auto copy = gst_buffer_copy(outbuf);
    try {
        self->RunInference(inbuf, copy);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during inference: %s", Utils::createNestedErrorMsg(e).c_str());
        gst_buffer_unref(inbuf);
        gst_buffer_unref(copy);
        return GST_FLOW_ERROR;
    }

    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static void gst_gva_tensor_inference_class_init(GstGvaTensorInferenceClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_tensor_inference_set_property;
    gobject_class->get_property = gst_gva_tensor_inference_get_property;
    gobject_class->dispose = gst_gva_tensor_inference_dispose;
    gobject_class->finalize = gst_gva_tensor_inference_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gst_gva_tensor_inference_set_caps;
    base_transform_class->sink_event = gst_gva_tensor_inference_sink_event;
    base_transform_class->transform_caps = gst_gva_tensor_inference_transform_caps;
    base_transform_class->transform_size = gst_gva_tensor_inference_transform_size;
    base_transform_class->start = gst_gva_tensor_inference_start;
    base_transform_class->stop = gst_gva_tensor_inference_stop;
    base_transform_class->transform = gst_gva_tensor_inference_transform;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_INFERENCE_NAME, "application",
                                          GVA_TENSOR_INFERENCE_DESCRIPTION, "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_TENSOR_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                            gst_caps_from_string(GVA_TENSOR_CAPS GVA_VAAPI_TENSOR_CAPS)));

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

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", NULL,
                            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
