/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaclassify.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define ELEMENT_DESCRIPTION "Object classification (requires GstVideoRegionOfInterestMeta on input)"

GST_DEBUG_CATEGORY_STATIC(gst_gva_classify_debug_category);
#define GST_CAT_DEFAULT gst_gva_classify_debug_category

#define DEFAULT_MODEL NULL
#define DEFAULT_INFERENCE_ID NULL
#define DEFAULT_MODEL_PROC NULL
#define DEFAULT_OBJECT_CLASS ""
#define DEFAULT_DEVICE "CPU"
#define DEFAULT_META_FORMAT ""
#define DEFAULT_CPU_EXTENSION ""
#define DEFAULT_GPU_EXTENSION ""
#define DEFAULT_RESIZE_BY_INFERENCE FALSE

#define DEFAULT_MIN_BATCH_SIZE 1
#define DEFAULT_MAX_BATCH_SIZE 1024
#define DEFAULT_BATCH_SIZE 1

#define DEFALUT_MIN_THRESHOLD 0.
#define DEFALUT_MAX_THRESHOLD 1.
#define DEFALUT_THRESHOLD 0.5

#define DEFAULT_MIN_EVERY_NTH_FRAME 1
#define DEFAULT_MAX_EVERY_NTH_FRAME UINT_MAX
#define DEFAULT_EVERY_NTH_FRAME 1

#define DEFAULT_MIN_NIREQ 1
#define DEFAULT_MAX_NIREQ 64
#define DEFAULT_NIREQ 2

#define DEFAULT_CPU_STREAMS ""
#define DEFAULT_USE_LANDMARKS FALSE

static void gst_gva_classify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_classify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_classify_dispose(GObject *object);
static void gst_gva_classify_finalize(GObject *object);

static gboolean gst_gva_classify_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);

static gboolean gst_gva_classify_start(GstBaseTransform *trans);
static gboolean gst_gva_classify_stop(GstBaseTransform *trans);
static gboolean gst_gva_classify_sink_event(GstBaseTransform *trans, GstEvent *event);

static GstFlowReturn gst_gva_classify_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_classify_cleanup(GstGvaClassify *gvaclassify);
static void gst_gva_classify_reset(GstGvaClassify *gvaclassify);
static GstStateChangeReturn gst_gva_classify_change_state(GstElement *element, GstStateChange transition);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_OBJECT_CLASS,
    PROP_DEVICE,
    PROP_BATCH_SIZE,
    PROP_EVERY_NTH_FRAME,
    PROP_NIREQ,
    PROP_CPU_EXTENSION,
    PROP_GPU_EXTENSION,
    PROP_INFERENCE_ID,
    PROP_MODEL_PROC,
    PROP_CPU_STREAMS,
    PROP_USE_LANDMARKS
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

G_DEFINE_TYPE_WITH_CODE(GstGvaClassify, gst_gva_classify, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_classify_debug_category, "gvaclassify", 0,
                                                "debug category for gvaclassify element"));

static void gst_gva_classify_class_init(GstGvaClassifyClass *klass) {
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

    gobject_class->set_property = gst_gva_classify_set_property;
    gobject_class->get_property = gst_gva_classify_get_property;
    gobject_class->dispose = gst_gva_classify_dispose;
    gobject_class->finalize = gst_gva_classify_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_classify_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_classify_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_classify_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_classify_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_classify_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_classify_change_state);

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model", "Inference model file path", DEFAULT_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_ID,
        g_param_spec_string("inference-id", "Inference Id",
                            "Id for the inference engine to be shared between ovino plugin instances",
                            DEFAULT_INFERENCE_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "JSON file with description of input/output layers pre-processing/post-processing",
                            DEFAULT_MODEL_PROC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_OBJECT_CLASS,
                                    g_param_spec_string("object-class", "ObjectClass", "Object class",
                                                        DEFAULT_OBJECT_CLASS,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_DEVICE,
                                    g_param_spec_string("device", "Device", "Inference device plugin CPU or GPU",
                                                        DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_CPU_EXTENSION,
                                    g_param_spec_string("cpu-extension", "CpuExtension", "Inference CPU extension",
                                                        DEFAULT_CPU_EXTENSION,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_GPU_EXTENSION,
                                    g_param_spec_string("gpu-extension", "GpuExtension", "Inference GPU extension",
                                                        DEFAULT_GPU_EXTENSION,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_BATCH_SIZE,
                                    g_param_spec_uint("batch-size", "Batch size", "number frames for batching",
                                                      DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE,
                                                      DEFAULT_BATCH_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_EVERY_NTH_FRAME,
                                    g_param_spec_uint("every-nth-frame", "EveryNthFrame", "Every Nth frame",
                                                      DEFAULT_MIN_EVERY_NTH_FRAME, DEFAULT_MAX_EVERY_NTH_FRAME,
                                                      DEFAULT_EVERY_NTH_FRAME,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "number of inference requests",
                                                      DEFAULT_MIN_NIREQ, DEFAULT_MAX_NIREQ, DEFAULT_NIREQ,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_USE_LANDMARKS,
        g_param_spec_boolean("use-landmarks", "landmarks usage flag", "Controls landmark usage preprocessing stage",
                             DEFAULT_USE_LANDMARKS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_CPU_STREAMS,
        g_param_spec_string("cpu-streams", "CPU-Streams",
                            "Use multiple inference streams/instances for better parallelization and affinity on CPU",
                            DEFAULT_CPU_STREAMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_classify_cleanup(GstGvaClassify *gvaclassify) {
    if (gvaclassify == NULL)
        return;

    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_cleanup");

    if (gvaclassify->inference) {
        release_classify_inference(gvaclassify);
        gvaclassify->inference = NULL;
    }

    g_free(gvaclassify->model);
    gvaclassify->model = NULL;

    g_free(gvaclassify->object_class);
    gvaclassify->device = NULL;

    g_free(gvaclassify->device);
    gvaclassify->device = NULL;

    g_free(gvaclassify->model_proc);
    gvaclassify->model_proc = NULL;

    g_free(gvaclassify->cpu_extension);
    gvaclassify->cpu_extension = NULL;

    g_free(gvaclassify->gpu_extension);
    gvaclassify->gpu_extension = NULL;

    g_free(gvaclassify->inference_id);
    gvaclassify->inference_id = NULL;

    if (gvaclassify->info) {
        gst_video_info_free(gvaclassify->info);
        gvaclassify->info = NULL;
    }

    gvaclassify->initialized = FALSE;
}

static void gst_gva_classify_reset(GstGvaClassify *gvaclassify) {
    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_reset");

    if (gvaclassify == NULL)
        return;

    gst_gva_classify_cleanup(gvaclassify);

    gvaclassify->model = g_strdup(DEFAULT_MODEL);
    gvaclassify->object_class = g_strdup(DEFAULT_OBJECT_CLASS);
    gvaclassify->device = g_strdup(DEFAULT_DEVICE);
    gvaclassify->model_proc = g_strdup(DEFAULT_MODEL_PROC);
    gvaclassify->batch_size = DEFAULT_BATCH_SIZE;
    gvaclassify->every_nth_frame = DEFAULT_EVERY_NTH_FRAME;
    gvaclassify->nireq = DEFAULT_NIREQ;
    gvaclassify->cpu_extension = g_strdup(DEFAULT_CPU_EXTENSION);
    gvaclassify->gpu_extension = g_strdup(DEFAULT_GPU_EXTENSION);
    gvaclassify->inference_id = g_strdup(DEFAULT_INFERENCE_ID);
    gvaclassify->cpu_streams = DEFAULT_CPU_STREAMS;
    gvaclassify->inference = NULL;
    gvaclassify->initialized = FALSE;
    gvaclassify->info = NULL;
    gvaclassify->use_landmarks = DEFAULT_USE_LANDMARKS;
}

static GstStateChangeReturn gst_gva_classify_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaClassify *gvaclassify;

    gvaclassify = GST_GVA_CLASSIFY(element);
    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_classify_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_classify_reset(gvaclassify);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_classify_init(GstGvaClassify *gvaclassify) {

    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_init");
    GST_DEBUG_OBJECT(gvaclassify, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvaclassify)));

    gst_gva_classify_reset(gvaclassify);
}

void gst_gva_classify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(object);

    GST_DEBUG_OBJECT(gvaclassify, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gvaclassify->model = g_value_dup_string(value);
        break;
    case PROP_OBJECT_CLASS:
        gvaclassify->object_class = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        gvaclassify->device = g_value_dup_string(value);
        break;
    case PROP_MODEL_PROC:
        gvaclassify->model_proc = g_value_dup_string(value);
        break;
    case PROP_BATCH_SIZE:
        gvaclassify->batch_size = g_value_get_uint(value);
        break;
    case PROP_EVERY_NTH_FRAME:
        gvaclassify->every_nth_frame = g_value_get_uint(value);
        break;
    case PROP_NIREQ:
        gvaclassify->nireq = g_value_get_uint(value);
        break;
    case PROP_CPU_EXTENSION:
        gvaclassify->cpu_extension = g_value_dup_string(value);
        break;
    case PROP_GPU_EXTENSION:
        gvaclassify->gpu_extension = g_value_dup_string(value);
        break;
    case PROP_INFERENCE_ID:
        gvaclassify->inference_id = g_value_dup_string(value);
        break;
    case PROP_CPU_STREAMS:
        gvaclassify->cpu_streams = g_value_dup_string(value);
        break;
    case PROP_USE_LANDMARKS:
        gvaclassify->use_landmarks = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_classify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(object);

    GST_DEBUG_OBJECT(gvaclassify, "get_property");

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_string(value, gvaclassify->model);
        break;
    case PROP_OBJECT_CLASS:
        g_value_set_string(value, gvaclassify->object_class);
        break;
    case PROP_DEVICE:
        g_value_set_string(value, gvaclassify->device);
        break;
    case PROP_MODEL_PROC:
        g_value_set_string(value, gvaclassify->model_proc);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, gvaclassify->batch_size);
        break;
    case PROP_EVERY_NTH_FRAME:
        g_value_set_uint(value, gvaclassify->every_nth_frame);
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, gvaclassify->nireq);
        break;
    case PROP_CPU_EXTENSION:
        g_value_set_string(value, gvaclassify->cpu_extension);
        break;
    case PROP_GPU_EXTENSION:
        g_value_set_string(value, gvaclassify->gpu_extension);
        break;
    case PROP_INFERENCE_ID:
        g_value_set_string(value, gvaclassify->inference_id);
        break;
    case PROP_CPU_STREAMS:
        g_value_set_string(value, gvaclassify->cpu_streams);
        break;
    case PROP_USE_LANDMARKS:
        g_value_set_boolean(value, gvaclassify->use_landmarks);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_classify_dispose(GObject *object) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(object);

    GST_DEBUG_OBJECT(gvaclassify, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_classify_parent_class)->dispose(object);
}

void gst_gva_classify_finalize(GObject *object) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(object);

    GST_DEBUG_OBJECT(gvaclassify, "finalize");

    /* clean up object here */
    gst_gva_classify_cleanup(gvaclassify);

    G_OBJECT_CLASS(gst_gva_classify_parent_class)->finalize(object);
}

static gboolean gst_gva_classify_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(trans);

    GST_DEBUG_OBJECT(gvaclassify, "set_caps");

    if (!gvaclassify->info) {
        gvaclassify->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gvaclassify->info, incaps);

    return TRUE;
}

/* states */
static gboolean gst_gva_classify_start(GstBaseTransform *trans) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(trans);

    GST_DEBUG_OBJECT(gvaclassify, "start");

    if (gvaclassify->initialized)
        goto exit;

    if (!gvaclassify->inference_id) {
        gvaclassify->inference_id = g_strdup(GST_ELEMENT_NAME(GST_ELEMENT(gvaclassify)));
    }

    GError *error = NULL;
    gvaclassify->inference = aquire_classify_inference(gvaclassify, &error);
    if (error) {
        GST_ELEMENT_ERROR(gvaclassify, RESOURCE, TOO_LAZY, ("gvaclassify plugin intitialization failed"),
                          ("%s", error->message));
        g_error_free(error);
        goto exit;
    }

    gvaclassify->initialized = TRUE;

exit:
    return gvaclassify->initialized;
}

static gboolean gst_gva_classify_stop(GstBaseTransform *trans) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(trans);

    GST_DEBUG_OBJECT(gvaclassify, "stop");
    // FIXME: Hangs when multichannel
    // flush_inference_classify(gvaclassify);

    return TRUE;
}

static gboolean gst_gva_classify_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(trans);

    GST_DEBUG_OBJECT(gvaclassify, "sink_event");

    classify_inference_sink_event(gvaclassify, event);

    return GST_BASE_TRANSFORM_CLASS(gst_gva_classify_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_classify_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(trans);

    GST_DEBUG_OBJECT(gvaclassify, "transform_ip");

    if (!gvaclassify->inference->instance) { // TODO find a way to move this check out to initialization stage
        GST_ELEMENT_ERROR(gvaclassify, RESOURCE, SETTINGS,
                          ("There is no master element provided for gvaclassify elements with inference-id '%s'. At "
                           "least one element for each inference-id should have model path specified",
                           gvaclassify->inference_id),
                          (NULL));
        return GST_FLOW_ERROR;
    }

    return frame_to_classify_inference(gvaclassify, trans, buf, gvaclassify->info);

    /* return GST_FLOW_OK; FIXME shouldn't signal about dropping frames in inplace transform function*/
}
