/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvainference.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <limits.h>

#include "gva_utils.h"

#include "config.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME                                                                                              \
    "Generic full-frame inference / object detection (generates GstGVATensorMeta / GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION                                                                                            \
    "Generic full-frame inference / object detection (generates GstGVATensorMeta / GstVideoRegionOfInterestMeta)"

GST_DEBUG_CATEGORY_STATIC(gst_gva_inference_debug_category);
#define GST_CAT_DEFAULT gst_gva_inference_debug_category

#define DEFAULT_MODEL NULL
#define DEFAULT_INFERENCE_ID NULL
#define DEFAULT_DEVICE "CPU"
#define DEFAULT_MODEL_PROC NULL
#define DEFAULT_CPU_EXTENSION ""
#define DEFAULT_GPU_EXTENSION ""
#define DEFAULT_RESIZE_BY_INFERENCE FALSE

#define DEFAULT_MIN_BATCH_SIZE 1
#define DEFAULT_MAX_BATCH_SIZE 1024
#define DEFAULT_BATCH_SIZE 1

#define DEFALUT_MIN_THRESHOLD 0.
#define DEFALUT_MAX_THRESHOLD 1.
#define DEFALUT_THRESHOLD 0.5

#define DEFAULT_MIN_EVERY_NTH_FRAME 0
#define DEFAULT_MAX_EVERY_NTH_FRAME UINT_MAX
#define DEFAULT_EVERY_NTH_FRAME 1

#define DEFAULT_MIN_NIREQ 1
#define DEFAULT_MAX_NIREQ 64
#define DEFAULT_NIREQ 2

#define DEFAULT_CPU_STREAMS ""

/* prototypes */
static void gst_gva_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_inference_dispose(GObject *object);
static void gst_gva_inference_finalize(GObject *object);

static gboolean gst_gva_inference_start(GstBaseTransform *trans);
static gboolean gst_gva_inference_stop(GstBaseTransform *trans);
static gboolean gst_gva_inference_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_gva_inference_sink_event(GstBaseTransform *trans, GstEvent *event);
static GstFlowReturn gst_gva_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_inference_cleanup(GstGvaInference *gvainference);
static void gst_gva_inference_reset(GstGvaInference *gvainference);
static GstStateChangeReturn gst_gva_inference_change_state(GstElement *element, GstStateChange transition);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_DEVICE,
    PROP_LABELS_FILE,
    PROP_MODEL_PROC,
    PROP_BATCH_SIZE,
    PROP_THRESHOLD,
    PROP_RESIZE_BY_INFERENCE,
    PROP_EVERY_NTH_FRAME,
    PROP_NIREQ,
    PROP_CPU_EXTENSION,
    PROP_GPU_EXTENSION,
    PROP_INFERENCE_ID,
    PROP_CPU_STREAMS,
    PROP_INFER_CONFIG,
};

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#ifndef DISABLE_VAAPI
#define VA_SURFACE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VASurface", "{ NV12 }") "; "
#else
#define VA_SURFACE_CAPS
#endif

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")
#define INFERENCE_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS INFERENCE_CAPS
#define VIDEO_SRC_CAPS INFERENCE_CAPS

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaInference, gst_gva_inference, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_inference_debug_category, "gvainference", 0,
                                                "debug category for gvainference element"));

static void gst_gva_inference_class_init(GstGvaInferenceClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_inference_set_property;
    gobject_class->get_property = gst_gva_inference_get_property;
    gobject_class->dispose = gst_gva_inference_dispose;
    gobject_class->finalize = gst_gva_inference_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_inference_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_inference_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_inference_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_inference_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_inference_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_inference_change_state);

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model", "Inference model file path", DEFAULT_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_ID,
        g_param_spec_string("inference-id", "Inference Id",
                            "Id for the inference engine to be shared between gvainference plugin instances",
                            DEFAULT_INFERENCE_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_DEVICE,
                                    g_param_spec_string("device", "Device", "Inference device plugin CPU or GPU",
                                                        DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "JSON file with description of input/output layers pre-processing/post-processing",
                            DEFAULT_MODEL_PROC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_CPU_EXTENSION,
                                    g_param_spec_string("cpu-extension", "CpuExtension", "Inference CPU extension",
                                                        DEFAULT_CPU_EXTENSION,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_GPU_EXTENSION,
                                    g_param_spec_string("gpu-extension", "GpuExtension", "Inference GPU extension",
                                                        DEFAULT_GPU_EXTENSION,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_RESIZE_BY_INFERENCE,
                                    g_param_spec_boolean("resize-by-inference", "ResizeByInference",
                                                         "OpenVINO does resizing", DEFAULT_RESIZE_BY_INFERENCE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_BATCH_SIZE,
                                    g_param_spec_uint("batch-size", "Batch size", "number frames for batching",
                                                      DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE,
                                                      DEFAULT_BATCH_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_float("threshold", "Threshold", "Threshold for inference",
                                                       DEFALUT_MIN_THRESHOLD, DEFALUT_MAX_THRESHOLD, DEFALUT_THRESHOLD,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_EVERY_NTH_FRAME,
                                    g_param_spec_uint("every-nth-frame", "EveryNthFrame", "Every Nth frame",
                                                      DEFAULT_MIN_EVERY_NTH_FRAME, DEFAULT_MAX_EVERY_NTH_FRAME,
                                                      DEFAULT_EVERY_NTH_FRAME,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests",
                                                      DEFAULT_MIN_NIREQ, DEFAULT_MAX_NIREQ, DEFAULT_NIREQ,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_CPU_STREAMS,
        g_param_spec_string("cpu-streams", "CPU-Streams",
                            "Use multiple inference streams/instances for better parallelization and affinity on CPU",
                            DEFAULT_CPU_STREAMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFER_CONFIG,
        g_param_spec_string("infer-config", "Infer-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", "",
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_inference_cleanup(GstGvaInference *gvainference) {
    if (gvainference == NULL)
        return;

    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_cleanup");

    if (gvainference->inference) {
        release_inference(gvainference);
        gvainference->inference = NULL;
    }

    g_free(gvainference->model);
    gvainference->model = NULL;

    g_free(gvainference->device);
    gvainference->device = NULL;

    g_free(gvainference->model_proc);
    gvainference->model_proc = NULL;

    g_free(gvainference->cpu_extension);
    gvainference->cpu_extension = NULL;

    g_free(gvainference->gpu_extension);
    gvainference->gpu_extension = NULL;

    g_free(gvainference->inference_id);
    gvainference->inference_id = NULL;

    g_free(gvainference->infer_config);
    gvainference->infer_config = NULL;

    if (gvainference->info) {
        gst_video_info_free(gvainference->info);
        gvainference->info = NULL;
    }

    gvainference->initialized = FALSE;
}

static void gst_gva_inference_reset(GstGvaInference *gvainference) {
    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_reset");

    if (gvainference == NULL)
        return;

    gst_gva_inference_cleanup(gvainference);

    gvainference->model = g_strdup(DEFAULT_MODEL);
    gvainference->device = g_strdup(DEFAULT_DEVICE);
    gvainference->model_proc = g_strdup(DEFAULT_MODEL_PROC);
    gvainference->batch_size = DEFAULT_BATCH_SIZE;
    gvainference->threshold = DEFALUT_THRESHOLD;
    gvainference->resize_by_inference = DEFAULT_RESIZE_BY_INFERENCE;
    gvainference->every_nth_frame = DEFAULT_EVERY_NTH_FRAME;
    gvainference->nireq = DEFAULT_NIREQ;
    gvainference->cpu_extension = g_strdup(DEFAULT_CPU_EXTENSION);
    gvainference->gpu_extension = g_strdup(DEFAULT_GPU_EXTENSION);
    gvainference->inference_id = g_strdup(DEFAULT_INFERENCE_ID);
    gvainference->infer_config = g_strdup("");
    gvainference->cpu_streams = DEFAULT_CPU_STREAMS;
    gvainference->inference = NULL;
    gvainference->initialized = FALSE;
    gvainference->info = NULL;
}

static GstStateChangeReturn gst_gva_inference_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaInference *gvainference;

    gvainference = GST_GVA_INFERENCE(element);
    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_inference_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_inference_reset(gvainference);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_inference_init(GstGvaInference *gvainference) {
    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_init");
    GST_DEBUG_OBJECT(gvainference, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvainference)));

    gst_gva_inference_reset(gvainference);
}

void gst_gva_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(object);

    GST_DEBUG_OBJECT(gvainference, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gvainference->model = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        gvainference->device = g_value_dup_string(value);
        break;
    case PROP_MODEL_PROC:
        gvainference->model_proc = g_value_dup_string(value);
        break;
    case PROP_BATCH_SIZE:
        gvainference->batch_size = g_value_get_uint(value);
        break;
    case PROP_THRESHOLD:
        gvainference->threshold = g_value_get_float(value);
        break;
    case PROP_RESIZE_BY_INFERENCE:
        gvainference->resize_by_inference = g_value_get_boolean(value);
        break;
    case PROP_EVERY_NTH_FRAME:
        gvainference->every_nth_frame = g_value_get_uint(value);
        break;
    case PROP_NIREQ:
        gvainference->nireq = g_value_get_uint(value);
        break;
    case PROP_CPU_EXTENSION:
        gvainference->cpu_extension = g_value_dup_string(value);
        break;
    case PROP_GPU_EXTENSION:
        gvainference->gpu_extension = g_value_dup_string(value);
        break;
    case PROP_INFERENCE_ID:
        gvainference->inference_id = g_value_dup_string(value);
        break;
    case PROP_INFER_CONFIG:
        gvainference->infer_config = g_value_dup_string(value);
        break;
    case PROP_CPU_STREAMS:
        gvainference->cpu_streams = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(object);

    GST_DEBUG_OBJECT(gvainference, "get_property");

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_string(value, gvainference->model);
        break;
    case PROP_DEVICE:
        g_value_set_string(value, gvainference->device);
        break;
    case PROP_MODEL_PROC:
        g_value_set_string(value, gvainference->model_proc);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, gvainference->batch_size);
        break;
    case PROP_THRESHOLD:
        g_value_set_float(value, gvainference->threshold);
        break;
    case PROP_RESIZE_BY_INFERENCE:
        g_value_set_boolean(value, gvainference->resize_by_inference);
        break;
    case PROP_EVERY_NTH_FRAME:
        g_value_set_uint(value, gvainference->every_nth_frame);
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, gvainference->nireq);
        break;
    case PROP_CPU_EXTENSION:
        g_value_set_string(value, gvainference->cpu_extension);
        break;
    case PROP_GPU_EXTENSION:
        g_value_set_string(value, gvainference->gpu_extension);
        break;
    case PROP_INFERENCE_ID:
        g_value_set_string(value, gvainference->inference_id);
        break;
    case PROP_INFER_CONFIG:
        g_value_set_string(value, gvainference->infer_config);
        break;
    case PROP_CPU_STREAMS:
        g_value_set_string(value, gvainference->cpu_streams);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_inference_dispose(GObject *object) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(object);

    GST_DEBUG_OBJECT(gvainference, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_inference_parent_class)->dispose(object);
}

void gst_gva_inference_finalize(GObject *object) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(object);

    GST_DEBUG_OBJECT(gvainference, "finalize");

    /* clean up object here */
    gst_gva_inference_cleanup(gvainference);

    G_OBJECT_CLASS(gst_gva_inference_parent_class)->finalize(object);
}

static gboolean gst_gva_inference_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaInference *gvainference = GST_GVA_INFERENCE(trans);

    GST_DEBUG_OBJECT(gvainference, "set_caps");

    if (!gvainference->info) {
        gvainference->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gvainference->info, incaps);

    return TRUE;
}

static gboolean gst_gva_inference_start(GstBaseTransform *trans) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(trans);

    GST_DEBUG_OBJECT(gvainference, "start");

    if (gvainference->initialized)
        goto exit;
    if (!gvainference->inference_id) {
        gvainference->inference_id = g_strdup(GST_ELEMENT_NAME(GST_ELEMENT(gvainference)));
    }

    GError *error = NULL;
    gvainference->inference = aquire_inference(gvainference, &error);
    if (error) {
        GST_ELEMENT_ERROR(gvainference, RESOURCE, TOO_LAZY, ("gvainference plugin intitialization failed"),
                          ("%s", error->message));
        g_error_free(error);
        goto exit;
    }

    gvainference->initialized = TRUE;

exit:
    return gvainference->initialized;
}

static gboolean gst_gva_inference_stop(GstBaseTransform *trans) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(trans);

    GST_DEBUG_OBJECT(gvainference, "stop");
    // FIXME: Hangs when multichannel
    // flush_inference(gvainference);

    return TRUE;
}

static gboolean gst_gva_inference_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(trans);

    GST_DEBUG_OBJECT(gvainference, "sink_event");

    inference_sink_event(gvainference, event);

    return GST_BASE_TRANSFORM_CLASS(gst_gva_inference_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(trans);

    GST_DEBUG_OBJECT(gvainference, "transform_ip");

    if (!gvainference->inference->instance) { // TODO find a way to move this check out to initialization stage
        GST_ELEMENT_ERROR(gvainference, RESOURCE, SETTINGS,
                          ("There is no master element provided for gvainference elements with inference-id"
                           "'%s'. At least one element for each inference-id should have model path specified",
                           gvainference->inference_id),
                          (NULL));
        return GST_FLOW_ERROR;
    }

    return frame_to_inference(gvainference, trans, buf, gvainference->info);

    /* return GST_FLOW_OK; FIXME shouldn't signal about dropping frames in inplace transform function*/
}
