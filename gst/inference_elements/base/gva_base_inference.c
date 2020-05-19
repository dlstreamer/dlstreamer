/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_base_inference.h"

#define DEFAULT_MODEL NULL
#define DEFAULT_MODEL_INSTANCE_ID NULL
#define DEFAULT_MODEL_PROC NULL
#define DEFAULT_DEVICE "CPU"
#define DEFAULT_DEVICE_EXTENSIONS ""
#define DEFAULT_PRE_PROC "ie"

#define DEFAULT_MIN_THRESHOLD 0.
#define DEFAULT_MAX_THRESHOLD 1.
#define DEFAULT_THRESHOLD 0.5

#define DEFAULT_MIN_INFERENCE_INTERVAL 1
#define DEFAULT_MAX_INFERENCE_INTERVAL UINT_MAX
#define DEFAULT_INFERENCE_INTERVAL 1

#define DEFAULT_RESHAPE FALSE

#define DEFAULT_MIN_BATCH_SIZE 1
#define DEFAULT_MAX_BATCH_SIZE 1024
#define DEFAULT_BATCH_SIZE 1

#define DEFAULT_MIN_RESHAPE_WIDTH 0
#define DEFAULT_MAX_RESHAPE_WIDTH UINT_MAX
#define DEFAULT_RESHAPE_WIDTH 0

#define DEFAULT_MIN_RESHAPE_HEIGHT 0
#define DEFAULT_MAX_RESHAPE_HEIGHT UINT_MAX
#define DEFAULT_RESHAPE_HEIGHT 0

#define DEFAULT_NO_BLOCK FALSE

#define DEFAULT_MIN_NIREQ 0
#define DEFAULT_MAX_NIREQ 1024
#define DEFAULT_NIREQ 0

#define DEFAULT_CPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MIN_CPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MAX_CPU_THROUGHPUT_STREAMS UINT_MAX

#define DEFAULT_GPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MIN_GPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MAX_GPU_THROUGHPUT_STREAMS UINT_MAX

#define DEFAULT_ALLOCATOR_NAME NULL

#define UNUSED(x) (void)(x)

G_DEFINE_TYPE(GvaBaseInference, gva_base_inference, GST_TYPE_BASE_TRANSFORM);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_DEVICE,
    PROP_INFERENCE_INTERVAL,
    PROP_RESHAPE,
    PROP_BATCH_SIZE,
    PROP_RESHAPE_WIDTH,
    PROP_RESHAPE_HEIGHT,
    PROP_NO_BLOCK,
    PROP_NIREQ,
    PROP_MODEL_INSTANCE_ID,
    PROP_PRE_PROC_BACKEND,
    PROP_MODEL_PROC,
    PROP_CPU_THROUGHPUT_STREAMS,
    PROP_GPU_THROUGHPUT_STREAMS,
    PROP_IE_CONFIG,
    PROP_DEVICE_EXTENSIONS
};

static void gva_base_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gva_base_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gva_base_inference_dispose(GObject *object);
static void gva_base_inference_finalize(GObject *object);

static gboolean gva_base_inference_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gva_base_inference_start(GstBaseTransform *trans);
static gboolean gva_base_inference_stop(GstBaseTransform *trans);
static gboolean gva_base_inference_sink_event(GstBaseTransform *trans, GstEvent *event);

static GstFlowReturn gva_base_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gva_base_inference_cleanup(GvaBaseInference *base_inference);
static GstStateChangeReturn gva_base_inference_change_state(GstElement *element, GstStateChange transition);

static void gva_base_inference_init(GvaBaseInference *base_inference);

static void gva_base_inference_class_init(GvaBaseInferenceClass *klass);

void gva_base_inference_class_init(GvaBaseInferenceClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gobject_class->set_property = gva_base_inference_set_property;
    gobject_class->get_property = gva_base_inference_get_property;
    gobject_class->dispose = gva_base_inference_dispose;
    gobject_class->finalize = gva_base_inference_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gva_base_inference_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gva_base_inference_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gva_base_inference_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gva_base_inference_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gva_base_inference_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gva_base_inference_change_state);

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model", "Path to inference model network file",
                                                        DEFAULT_MODEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_INSTANCE_ID,
        g_param_spec_string(
            "model-instance-id", "Model Instance Id",
            "Identifier for sharing a loaded model instance between elements of the same type. Elements with the "
            "same model-instance-id will share all model and inference engine related properties",
            DEFAULT_MODEL_INSTANCE_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_PRE_PROC_BACKEND,
        g_param_spec_string(
            "pre-process-backend", "Pre-processing method",
            "Select a pre-processing method (color conversion and resize), one of 'ie', 'opencv', 'vaapi'",
            DEFAULT_PRE_PROC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "Path to JSON file with description of input/output layers pre-processing/post-processing",
                            DEFAULT_MODEL_PROC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Device",
            "Target device for inference. Please see OpenVINO documentation for list of supported devices.",
            DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_BATCH_SIZE,
                                    g_param_spec_uint("batch-size", "Batch size",
                                                      "Number of frames batched together for a single inference. "
                                                      "Not all models support batching. Use model optimizer to ensure "
                                                      "that the model has batching support.",
                                                      DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE,
                                                      DEFAULT_BATCH_SIZE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_INTERVAL,
        g_param_spec_uint("inference-interval", "Inference Interval",
                          "Interval between inference requests. An interval of 1 (Default) performs inference on "
                          "every frame. An interval of 2 performs inference on every other frame. An interval of N "
                          "performs inference on every Nth frame.",
                          DEFAULT_MIN_INFERENCE_INTERVAL, DEFAULT_MAX_INFERENCE_INTERVAL, DEFAULT_INFERENCE_INTERVAL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE,
        g_param_spec_boolean("reshape", "Reshape input layer",
                             "Enable network reshaping.  "
                             "Use only 'reshape=true' without reshape-width and reshape-height properties "
                             "if you want to reshape network to the original size of input frames. "
                             "Note: this feature has a set of limitations. "
                             "Before use, make sure that your network supports reshaping",
                             DEFAULT_RESHAPE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE_WIDTH,
        g_param_spec_uint("reshape-width", "Width for reshape", "Width to which the network will be reshaped.",
                          DEFAULT_MIN_RESHAPE_WIDTH, DEFAULT_MAX_RESHAPE_WIDTH, DEFAULT_RESHAPE_WIDTH,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE_HEIGHT,
        g_param_spec_uint("reshape-height", "Height for reshape", "Height to which the network will be reshaped.",
                          DEFAULT_MIN_RESHAPE_HEIGHT, DEFAULT_MAX_RESHAPE_HEIGHT, DEFAULT_RESHAPE_HEIGHT,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_NO_BLOCK,
        g_param_spec_boolean(
            "no-block", "Adaptive inference skipping",
            "(Experimental) Option to help maintain frames per second of incoming stream. Skips inference "
            "on an incoming frame if all inference requests are currently processing outstanding frames",
            DEFAULT_NO_BLOCK, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests",
                                                      DEFAULT_MIN_NIREQ, DEFAULT_MAX_NIREQ, DEFAULT_NIREQ,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_CPU_THROUGHPUT_STREAMS,
        g_param_spec_uint(
            "cpu-throughput-streams", "CPU-Throughput-Streams",
            "Sets the cpu-throughput-streams configuration key for OpenVINO's "
            "cpu device plugin. Configuration allows for multiple inference streams "
            "for better performance. Default mode is auto. See OpenVINO CPU plugin documentation for more details",
            DEFAULT_MIN_CPU_THROUGHPUT_STREAMS, DEFAULT_MAX_CPU_THROUGHPUT_STREAMS, DEFAULT_CPU_THROUGHPUT_STREAMS,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_GPU_THROUGHPUT_STREAMS,
        g_param_spec_uint(
            "gpu-throughput-streams", "GPU-Throughput-Streams",
            "Sets the gpu-throughput-streams configuration key for OpenVINO's "
            "gpu device plugin. Configuration allows for multiple inference streams "
            "for better performance. Default mode is auto. See OpenVINO GPU plugin documentation for more details",
            DEFAULT_MIN_GPU_THROUGHPUT_STREAMS, DEFAULT_MAX_GPU_THROUGHPUT_STREAMS, DEFAULT_GPU_THROUGHPUT_STREAMS,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", "",
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE_EXTENSIONS,
        g_param_spec_string(
            "device-extensions", "ExtensionString",
            "Comma separated list of KEY=VALUE pairs specifying the OpenVINO Inference Engine extension for a device",
            DEFAULT_DEVICE_EXTENSIONS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

void gva_base_inference_cleanup(GvaBaseInference *base_inference) {
    if (base_inference == NULL)
        return;

    GST_DEBUG_OBJECT(base_inference, "gva_base_inference_cleanup");

    if (base_inference->inference) {
        release_inference_instance(base_inference);
        base_inference->inference = NULL;
    }

    g_free(base_inference->model);
    base_inference->model = NULL;

    g_free(base_inference->device);
    base_inference->device = NULL;

    g_free(base_inference->model_proc);
    base_inference->model_proc = NULL;

    g_free(base_inference->model_instance_id);
    base_inference->model_instance_id = NULL;

    g_free(base_inference->pre_proc_name);
    base_inference->pre_proc_name = NULL;

    g_free(base_inference->ie_config);
    base_inference->ie_config = NULL;

    g_free(base_inference->allocator_name);
    base_inference->allocator_name = NULL;

    g_free(base_inference->pre_proc_name);
    base_inference->pre_proc_name = NULL;

    g_free(base_inference->device_extensions);
    base_inference->device_extensions = NULL;

    if (base_inference->info) {
        gst_video_info_free(base_inference->info);
        base_inference->info = NULL;
    }

    base_inference->initialized = FALSE;

    base_inference->num_skipped_frames = UINT_MAX - 1; // always run inference on first frame
}

void gva_base_inference_init(GvaBaseInference *base_inference) {
    GST_DEBUG_OBJECT(base_inference, "gva_base_inference_reset");

    if (base_inference == NULL)
        return;

    gva_base_inference_cleanup(base_inference);

    // Property
    base_inference->model = g_strdup(DEFAULT_MODEL);
    base_inference->device = g_strdup(DEFAULT_DEVICE);
    base_inference->model_proc = g_strdup(DEFAULT_MODEL_PROC);
    base_inference->inference_interval = DEFAULT_INFERENCE_INTERVAL;
    base_inference->reshape = DEFAULT_RESHAPE;
    base_inference->batch_size = DEFAULT_BATCH_SIZE;
    base_inference->reshape_width = DEFAULT_RESHAPE_WIDTH;
    base_inference->reshape_height = DEFAULT_RESHAPE_HEIGHT;
    base_inference->no_block = DEFAULT_NO_BLOCK;
    base_inference->nireq = DEFAULT_NIREQ;
    base_inference->model_instance_id = g_strdup(DEFAULT_MODEL_INSTANCE_ID);
    base_inference->pre_proc_name = g_strdup(DEFAULT_PRE_PROC);
    // TODO: make one property for streams
    base_inference->cpu_streams = DEFAULT_CPU_THROUGHPUT_STREAMS;
    base_inference->gpu_streams = DEFAULT_GPU_THROUGHPUT_STREAMS;
    base_inference->ie_config = g_strdup("");
    base_inference->allocator_name = g_strdup(DEFAULT_ALLOCATOR_NAME);
    base_inference->device_extensions = g_strdup(DEFAULT_DEVICE_EXTENSIONS);

    base_inference->initialized = FALSE;
    base_inference->info = NULL;
    base_inference->is_full_frame = TRUE;
    base_inference->inference = NULL;
    base_inference->is_roi_classification_needed = NULL;
    base_inference->pre_proc = NULL;
    base_inference->get_roi_pre_proc = NULL;
    base_inference->post_proc = NULL;
}

GstStateChangeReturn gva_base_inference_change_state(GstElement *element, GstStateChange transition) {
    GvaBaseInference *base_inference;

    base_inference = GVA_BASE_INFERENCE(element);
    GST_DEBUG_OBJECT(base_inference, "gva_base_inference_change_state");

    return GST_ELEMENT_CLASS(gva_base_inference_parent_class)->change_state(element, transition);
}

gboolean check_gva_base_inference_stopped(GvaBaseInference *base_inference) {
    GstState state;
    gboolean is_stopped;

    GST_OBJECT_LOCK(base_inference);
    state = GST_STATE(base_inference);
    is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
    GST_OBJECT_UNLOCK(base_inference);
    return is_stopped;
}

void gva_base_inference_set_model(GvaBaseInference *base_inference, const gchar *model_path) {
    if (check_gva_base_inference_stopped(base_inference)) {
        g_free(base_inference->model);
        base_inference->model = g_strdup(model_path);
        GST_INFO_OBJECT(base_inference, "model: %s", base_inference->model);
    } else {
        GST_ELEMENT_ERROR(base_inference, RESOURCE, SETTINGS, ("'model' can't be changed"),
                          ("You cannot change 'model' property on base_inference when a file is open"));
    }
}

void gva_base_inference_set_model_proc(GvaBaseInference *base_inference, const gchar *model_proc_path) {
    if (check_gva_base_inference_stopped(base_inference)) {
        g_free(base_inference->model_proc);
        base_inference->model_proc = g_strdup(model_proc_path);
        GST_INFO_OBJECT(base_inference, "model-proc: %s", base_inference->model_proc);
    } else {
        GST_ELEMENT_WARNING(base_inference, RESOURCE, SETTINGS, ("'model-proc' can't be changed"),
                            ("You cannot change 'model-proc' property on base_inference when a file is open"));
    }
}

void gva_base_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(object);

    GST_DEBUG_OBJECT(base_inference, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gva_base_inference_set_model(base_inference, g_value_get_string(value));
        break;
    case PROP_MODEL_PROC:
        gva_base_inference_set_model_proc(base_inference, g_value_get_string(value));
        break;
    case PROP_DEVICE:
        g_free(base_inference->device);
        base_inference->device = g_value_dup_string(value);
        break;
    case PROP_INFERENCE_INTERVAL:
        base_inference->inference_interval = g_value_get_uint(value);
        break;
    case PROP_RESHAPE:
        base_inference->reshape = g_value_get_boolean(value);
        break;
    case PROP_EVERY_NTH_FRAME:
        base_inference->every_nth_frame = g_value_get_uint(value);
        break;
    case PROP_RESHAPE:
        base_inference->reshape = g_value_get_boolean(value);
        break;
    case PROP_BATCH_SIZE:
        base_inference->batch_size = g_value_get_uint(value);
        if (base_inference->batch_size != DEFAULT_BATCH_SIZE)
            base_inference->reshape = TRUE;
        break;
    case PROP_RESHAPE_WIDTH:
        base_inference->reshape_width = g_value_get_uint(value);
        if (base_inference->reshape_width != DEFAULT_RESHAPE_WIDTH)
            base_inference->reshape = TRUE;
        break;
    case PROP_RESHAPE_HEIGHT:
        base_inference->reshape_height = g_value_get_uint(value);
        if (base_inference->reshape_height != DEFAULT_RESHAPE_HEIGHT)
            base_inference->reshape = TRUE;
        break;
    case PROP_NO_BLOCK:
        base_inference->no_block = g_value_get_boolean(value);
        break;
    case PROP_NIREQ:
        base_inference->nireq = g_value_get_uint(value);
        break;
    case PROP_MODEL_INSTANCE_ID:
        g_free(base_inference->model_instance_id);
        base_inference->model_instance_id = g_value_dup_string(value);
        break;
    case PROP_PRE_PROC_BACKEND:
        g_free(base_inference->pre_proc_name);
        base_inference->pre_proc_name = g_value_dup_string(value);
        break;
    case PROP_CPU_THROUGHPUT_STREAMS:
        base_inference->cpu_streams = g_value_get_uint(value);
        break;
    case PROP_GPU_THROUGHPUT_STREAMS:
        base_inference->gpu_streams = g_value_get_uint(value);
        break;
    case PROP_IE_CONFIG:
        g_free(base_inference->ie_config);
        base_inference->ie_config = g_value_dup_string(value);
        break;
    case PROP_DEVICE_EXTENSIONS:
        g_free(base_inference->device_extensions);
        base_inference->device_extensions = g_value_dup_string(value);
        break;
    case PROP_EXTENSION:
        base_inference->extension = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gva_base_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(object);

    GST_DEBUG_OBJECT(base_inference, "get_property");

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_string(value, base_inference->model);
        break;
    case PROP_MODEL_PROC:
        g_value_set_string(value, base_inference->model_proc);
        break;
    case PROP_DEVICE:
        g_value_set_string(value, base_inference->device);
        break;
    case PROP_INFERENCE_INTERVAL:
        g_value_set_uint(value, base_inference->inference_interval);
        break;
    case PROP_RESHAPE:
        g_value_set_boolean(value, base_inference->reshape);
        break;
    case PROP_EVERY_NTH_FRAME:
        g_value_set_uint(value, base_inference->every_nth_frame);
        break;
    case PROP_RESHAPE:
        g_value_set_boolean(value, base_inference->reshape);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, base_inference->batch_size);
        break;
    case PROP_RESHAPE_WIDTH:
        g_value_set_uint(value, base_inference->reshape_width);
        break;
    case PROP_RESHAPE_HEIGHT:
        g_value_set_uint(value, base_inference->reshape_height);
        break;
    case PROP_NO_BLOCK:
        g_value_set_boolean(value, base_inference->no_block);
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, base_inference->nireq);
        break;
    case PROP_MODEL_INSTANCE_ID:
        g_value_set_string(value, base_inference->model_instance_id);
        break;
    case PROP_PRE_PROC_BACKEND:
        g_value_set_string(value, base_inference->pre_proc_name);
        break;
    case PROP_CPU_THROUGHPUT_STREAMS:
        g_value_set_uint(value, base_inference->cpu_streams);
        break;
    case PROP_GPU_THROUGHPUT_STREAMS:
        g_value_set_uint(value, base_inference->gpu_streams);
        break;
    case PROP_IE_CONFIG:
        g_value_set_string(value, base_inference->ie_config);
        break;
    case PROP_DEVICE_EXTENSIONS:
        g_value_set_string(value, base_inference->device_extensions);
        break;
    case PROP_EXTENSION:
        g_value_set_string(value, base_inference->extension);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gva_base_inference_dispose(GObject *object) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(object);

    GST_DEBUG_OBJECT(base_inference, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gva_base_inference_parent_class)->dispose(object);
}

void gva_base_inference_finalize(GObject *object) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(object);

    GST_DEBUG_OBJECT(base_inference, "finalize");

    /* clean up object here */
    gva_base_inference_cleanup(base_inference);

    G_OBJECT_CLASS(gva_base_inference_parent_class)->finalize(object);
}

gboolean gva_base_inference_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "set_caps");

    if (!base_inference->info) {
        base_inference->info = gst_video_info_new();
    }
    gst_video_info_from_caps(base_inference->info, incaps);

    base_inference->inference = acquire_inference_instance(base_inference);

    return base_inference->inference != NULL;
}

gboolean gva_base_inference_start(GstBaseTransform *trans) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "start");

    if (!base_inference->model_instance_id) {
        base_inference->model_instance_id = g_strdup(GST_ELEMENT_NAME(GST_ELEMENT(base_inference)));

        if (base_inference->model == NULL) {
            GST_ELEMENT_ERROR(base_inference, RESOURCE, NOT_FOUND, ("'model' is not set"),
                              ("'model' property is not set"));
            goto exit;
        } else if (!g_file_test(base_inference->model, G_FILE_TEST_EXISTS)) {
            GST_ELEMENT_ERROR(base_inference, RESOURCE, NOT_FOUND, ("'model' does not exist"),
                              ("path %s set in 'model' does not exist", base_inference->model));
            goto exit;
        }
    }

    if (base_inference->model_proc != NULL && !g_file_test(base_inference->model_proc, G_FILE_TEST_EXISTS)) {
        GST_ELEMENT_WARNING(base_inference, RESOURCE, NOT_FOUND, ("'model-proc' does not exist"),
                            ("path %s set in 'model-proc' does not exist", base_inference->model_proc));
    }

    gboolean success = registerElement(base_inference);
    if (!success)
        goto exit;

    base_inference->initialized = TRUE;

exit:
    return base_inference->initialized;
}

gboolean gva_base_inference_stop(GstBaseTransform *trans) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "stop");
    // FIXME: Hangs when multichannel
    // flush_inference(base_inference);

    return TRUE;
}

gboolean gva_base_inference_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "sink_event");

    base_inference_sink_event(base_inference, event);

    return GST_BASE_TRANSFORM_CLASS(gva_base_inference_parent_class)->sink_event(trans, event);
}

GstFlowReturn gva_base_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "transform_ip");

    if (!base_inference->inference) { // TODO find a way to move this check out to initialization stage
        GST_ELEMENT_ERROR(base_inference, RESOURCE, SETTINGS,
                          ("There is no master element provided for base_inference elements with inference-id '%s'. At "
                           "least one element for each inference-id should have model path specified",
                           base_inference->model_instance_id),
                          (NULL));
        return GST_FLOW_ERROR;
    }

    return frame_to_base_inference(base_inference, buf, base_inference->info);

    /* return GST_FLOW_OK; FIXME shouldn't signal about dropping frames in inplace transform function*/
}
