/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvagenai.h"

#include <gst/video/video.h>
#include <gva_json_meta.h>

#include "genai.hpp"

GST_DEBUG_CATEGORY(gst_gvagenai_debug);
#define GST_CAT_DEFAULT gst_gvagenai_debug

// Element property definitions
enum {
    PROP_0,
    PROP_DEVICE,
    PROP_MODEL_PATH,
    PROP_PROMPT,
    PROP_GENERATION_CONFIG,
    PROP_SCHEDULER_CONFIG,
    PROP_MODEL_CACHE_PATH,
    PROP_FRAME_RATE,
    PROP_CHUNK_SIZE,
    PROP_METRICS
};

// Pad templates
static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGB, RGBA, RGBx, BGR, BGRA, BGRx, NV12, I420 }")));

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE("{ RGB, RGBA, RGBx, BGR, BGRA, BGRx, NV12, I420 }")));

// Class initialization
G_DEFINE_TYPE(GstGvaGenAI, gst_gvagenai, GST_TYPE_BASE_TRANSFORM);

// GObject vmethod implementations
static void gst_gvagenai_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gvagenai_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_gvagenai_finalize(GObject *object);

// GstBaseTransform vmethod implementations
static gboolean gst_gvagenai_start(GstBaseTransform *base);
static gboolean gst_gvagenai_stop(GstBaseTransform *base);
static GstFlowReturn gst_gvagenai_transform_ip(GstBaseTransform *base, GstBuffer *buf);
static gboolean gst_gvagenai_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps);

// Initialize the element class
static void gst_gvagenai_class_init(GstGvaGenAIClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    // Setting up pads and setting metadata
    gst_element_class_add_static_pad_template(element_class, &src_template);
    gst_element_class_add_static_pad_template(element_class, &sink_template);

    gst_element_class_set_static_metadata(element_class, "OpenVINO™ GenAI Inference", "Video/AI",
                                          "Runs OpenVINO™ GenAI inference on video frames", "Intel Corporation");

    // Set virtual methods
    gobject_class->set_property = gst_gvagenai_set_property;
    gobject_class->get_property = gst_gvagenai_get_property;
    gobject_class->finalize = gst_gvagenai_finalize;

    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gvagenai_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gvagenai_stop);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gvagenai_transform_ip);
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gvagenai_set_caps);

    // Install properties
    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string("device", "Device", "Device to use (CPU, GPU, NPU, etc.)", "CPU", G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_PATH,
        g_param_spec_string("model-path", "Model Path", "Path to the GenAI model", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(
        gobject_class, PROP_PROMPT,
        g_param_spec_string("prompt", "Prompt", "Text prompt for the GenAI model", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_GENERATION_CONFIG,
                                    g_param_spec_string("generation-config", "Generation Config",
                                                        "Generation configuration as KEY=VALUE,KEY=VALUE format", NULL,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SCHEDULER_CONFIG,
                                    g_param_spec_string("scheduler-config", "Scheduler Config",
                                                        "Scheduler configuration as KEY=VALUE,KEY=VALUE format", NULL,
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_MODEL_CACHE_PATH,
                                    g_param_spec_string("model-cache-path", "Model Cache Path",
                                                        "Path for caching compiled models (GPU only)", "ov_cache",
                                                        G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FRAME_RATE,
                                    g_param_spec_double("frame-rate", "Frame Rate",
                                                        "Number of frames sampled per second for inference "
                                                        "(0 = process all frames)",
                                                        0.0, G_MAXDOUBLE, 0.0, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CHUNK_SIZE,
                                    g_param_spec_uint("chunk-size", "Chunk Size", "Number of frames in one inference",
                                                      1, G_MAXUINT, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_METRICS,
                                    g_param_spec_boolean("metrics", "Metrics",
                                                         "Include performance metrics in JSON output", FALSE,
                                                         G_PARAM_READWRITE));

    GST_DEBUG_CATEGORY_INIT(gst_gvagenai_debug, "gvagenai", 0, "OpenVINO™ GenAI Inference");
}

/* Initialize the instance */
static void gst_gvagenai_init(GstGvaGenAI *gvagenai) {
    gvagenai->device = g_strdup("CPU");
    gvagenai->model_path = NULL;
    gvagenai->prompt = NULL;
    gvagenai->generation_config = NULL;
    gvagenai->scheduler_config = NULL;
    gvagenai->model_cache_path = g_strdup("ov_cache");
    gvagenai->frame_rate = 0.0; // Process all frames by default
    gvagenai->chunk_size = 1;   // Process one frame at a time by default
    gvagenai->metrics = FALSE;
    gvagenai->frame_counter = 0;
    gvagenai->openvino_context = NULL;
}

static void gst_gvagenai_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(object);

    switch (prop_id) {
    case PROP_DEVICE:
        g_free(gvagenai->device);
        gvagenai->device = g_value_dup_string(value);
        break;
    case PROP_MODEL_PATH:
        g_free(gvagenai->model_path);
        gvagenai->model_path = g_value_dup_string(value);
        break;
    case PROP_PROMPT:
        g_free(gvagenai->prompt);
        gvagenai->prompt = g_value_dup_string(value);
        break;
    case PROP_GENERATION_CONFIG:
        g_free(gvagenai->generation_config);
        gvagenai->generation_config = g_value_dup_string(value);
        break;
    case PROP_SCHEDULER_CONFIG:
        g_free(gvagenai->scheduler_config);
        gvagenai->scheduler_config = g_value_dup_string(value);
        break;
    case PROP_MODEL_CACHE_PATH:
        g_free(gvagenai->model_cache_path);
        gvagenai->model_cache_path = g_value_dup_string(value);
        break;
    case PROP_FRAME_RATE:
        gvagenai->frame_rate = g_value_get_double(value);
        gvagenai->frame_counter = 0; // Reset counter when changing frame rate
        break;
    case PROP_CHUNK_SIZE:
        gvagenai->chunk_size = g_value_get_uint(value);
        break;
    case PROP_METRICS:
        gvagenai->metrics = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gvagenai_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(object);

    switch (prop_id) {
    case PROP_DEVICE:
        g_value_set_string(value, gvagenai->device);
        break;
    case PROP_MODEL_PATH:
        g_value_set_string(value, gvagenai->model_path);
        break;
    case PROP_PROMPT:
        g_value_set_string(value, gvagenai->prompt);
        break;
    case PROP_GENERATION_CONFIG:
        g_value_set_string(value, gvagenai->generation_config);
        break;
    case PROP_SCHEDULER_CONFIG:
        g_value_set_string(value, gvagenai->scheduler_config);
        break;
    case PROP_MODEL_CACHE_PATH:
        g_value_set_string(value, gvagenai->model_cache_path);
        break;
    case PROP_FRAME_RATE:
        g_value_set_double(value, gvagenai->frame_rate);
        break;
    case PROP_CHUNK_SIZE:
        g_value_set_uint(value, gvagenai->chunk_size);
        break;
    case PROP_METRICS:
        g_value_set_boolean(value, gvagenai->metrics);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gvagenai_finalize(GObject *object) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(object);

    g_free(gvagenai->device);
    g_free(gvagenai->model_path);
    g_free(gvagenai->prompt);
    g_free(gvagenai->generation_config);
    g_free(gvagenai->scheduler_config);
    g_free(gvagenai->model_cache_path);

    // Clean up context
    if (gvagenai->openvino_context) {
        delete static_cast<genai::OpenVINOGenAIContext *>(gvagenai->openvino_context);
        gvagenai->openvino_context = NULL;
    }

    G_OBJECT_CLASS(gst_gvagenai_parent_class)->finalize(object);
}

static gboolean gst_gvagenai_start(GstBaseTransform *base) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(base);

    if (!gvagenai->model_path) {
        GST_ERROR_OBJECT(gvagenai, "Model path not specified");
        return FALSE;
    }

    if (!gvagenai->prompt) {
        GST_ERROR_OBJECT(gvagenai, "Prompt not specified");
        return FALSE;
    }

    // Create and initialize context
    try {
        auto context =
            new genai::OpenVINOGenAIContext(gvagenai->model_path, gvagenai->device,
                                            gvagenai->model_cache_path ? gvagenai->model_cache_path : "ov_cache",
                                            gvagenai->generation_config ? gvagenai->generation_config : "",
                                            gvagenai->scheduler_config ? gvagenai->scheduler_config : "");
        gvagenai->openvino_context = context;
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(gvagenai, "Failed to initialize OpenVINO™ GenAI context: %s", e.what());
        return FALSE;
    }

    return TRUE;
}

static gboolean gst_gvagenai_stop(GstBaseTransform *base) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(base);

    if (gvagenai->openvino_context) {
        auto *context = static_cast<genai::OpenVINOGenAIContext *>(gvagenai->openvino_context);
        context->clear_tensor_vector();
        delete context;
        gvagenai->openvino_context = NULL;
    }

    return TRUE;
}

static GstFlowReturn gst_gvagenai_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    GstGvaGenAI *gvagenai = GST_GVAGENAI(base);

    if (!gvagenai->openvino_context) {
        GST_ERROR_OBJECT(gvagenai, "Context not initialized");
        return GST_FLOW_ERROR;
    }

    // Get video info from pad
    GstVideoInfo info;
    GstCaps *caps = gst_pad_get_current_caps(base->sinkpad);
    gst_video_info_from_caps(&info, caps);
    gst_caps_unref(caps);

    gvagenai->frame_counter++;

    // Calculate frame sampling based on frame_rate
    if (gvagenai->frame_rate > 0) {
        gdouble input_fps = (gdouble)info.fps_n / (gdouble)info.fps_d;
        guint frames_to_skip = (guint)std::ceil(input_fps / gvagenai->frame_rate);

        // Skip frames if needed
        if (frames_to_skip > 0 && (gvagenai->frame_counter % frames_to_skip != 0)) {
            GST_DEBUG_OBJECT(gvagenai, "Skipping frame %u based on frame rate %f", gvagenai->frame_counter,
                             gvagenai->frame_rate);
            return GST_FLOW_OK;
        }
    }

    auto *context = static_cast<genai::OpenVINOGenAIContext *>(gvagenai->openvino_context);

    // Convert frame to tensor and add to vector
    if (!context->add_tensor_to_vector(buf, &info)) {
        GST_ERROR_OBJECT(gvagenai, "Failed to add frame to tensor vector");
        return GST_FLOW_ERROR;
    }

    // Only process if we've accumulated enough tensors
    if (context->get_tensor_vector_size() >= gvagenai->chunk_size) {
        // Process tensor vector
        if (!context->inference_tensor_vector(gvagenai->prompt)) {
            GST_ERROR_OBJECT(gvagenai, "Failed to inference tensor vector");
            return GST_FLOW_ERROR;
        }

        // Add metadata with the result to the latest frame
        const GstMetaInfo *meta_info = gst_gva_json_meta_get_info();
        if (meta_info && gst_buffer_is_writable(buf)) {
            auto *json_meta = (GstGVAJSONMeta *)gst_buffer_add_meta(buf, meta_info, NULL);
            json_meta->message =
                g_strdup(context->create_json_metadata(GST_BUFFER_TIMESTAMP(buf), gvagenai->metrics).c_str());
            GST_INFO_OBJECT(gvagenai, "Added meta message: %s", json_meta->message);
        } else {
            GST_WARNING_OBJECT(gvagenai, "Buffer is not writable or failed to get meta info");
        }
    } else {
        GST_DEBUG_OBJECT(gvagenai, "Added tensor %u of %u", (guint)context->get_tensor_vector_size(),
                         gvagenai->chunk_size);
    }

    return GST_FLOW_OK;
}

static gboolean gst_gvagenai_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps) {
    // Validate that we can handle the input caps
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, incaps)) {
        GST_ERROR_OBJECT(base, "Failed to parse input caps");
        return FALSE;
    }

    // Check if the format is supported
    if (GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_RGB && GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_RGBA &&
        GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_RGBx && GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_BGR &&
        GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_BGRA &&
        GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_BGRx &&
        GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_NV12 &&
        GST_VIDEO_INFO_FORMAT(&info) != GST_VIDEO_FORMAT_I420) {
        GST_ERROR_OBJECT(base, "Unsupported format");
        return FALSE;
    }

    return TRUE;
}

static gboolean plugin_init(GstPlugin *plugin) {
    gst_element_register(plugin, "gvagenai", GST_RANK_NONE, GST_TYPE_GVAGENAI);
    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvagenai, PRODUCT_FULL_NAME " GenAI elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
