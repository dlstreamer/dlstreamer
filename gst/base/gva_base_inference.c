/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_base_inference.h"

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

#define DEFAULT_ALLOCATOR_NAME NULL

#define UNUSED(x) (void)(x)

G_DEFINE_TYPE(GvaBaseInference, gva_base_inference, GST_TYPE_BASE_TRANSFORM);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_OBJECT_CLASS,
    PROP_DEVICE,
    PROP_BATCH_SIZE,
    PROP_EVERY_NTH_FRAME,
    PROP_NIREQ,
    PROP_INFERENCE_ID,
    PROP_MODEL_PROC,
    PROP_CPU_STREAMS,
    PROP_INFER_CONFIG,
    PROP_ALLOCATOR_NAME
};

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
                                    g_param_spec_string("device", "Device", "Type of device for inference (CPU or GPU)",
                                                        DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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
        gobject_class, PROP_CPU_STREAMS,
        g_param_spec_string("cpu-streams", "CPU-Streams",
                            "Use multiple inference streams/instances for better parallelization and affinity on CPU",
                            DEFAULT_CPU_STREAMS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFER_CONFIG,
        g_param_spec_string("infer-config", "Infer-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", "",
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ALLOCATOR_NAME,
                                    g_param_spec_string("allocator-name", "AllocatorName",
                                                        "Registered allocator name to be used", DEFAULT_ALLOCATOR_NAME,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
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

    g_free(base_inference->object_class);
    base_inference->object_class = NULL;

    g_free(base_inference->device);
    base_inference->device = NULL;

    g_free(base_inference->model_proc);
    base_inference->model_proc = NULL;

    g_free(base_inference->inference_id);
    base_inference->inference_id = NULL;

    g_free(base_inference->infer_config);
    base_inference->infer_config = NULL;

    g_free(base_inference->allocator_name);
    base_inference->allocator_name = NULL;

    if (base_inference->info) {
        gst_video_info_free(base_inference->info);
        base_inference->info = NULL;
    }

    base_inference->initialized = FALSE;
}

void gva_base_inference_init(GvaBaseInference *base_inference) {
    GST_DEBUG_OBJECT(base_inference, "gva_base_inference_reset");

    if (base_inference == NULL)
        return;

    gva_base_inference_cleanup(base_inference);

    base_inference->model = g_strdup(DEFAULT_MODEL);
    base_inference->object_class = g_strdup(DEFAULT_OBJECT_CLASS);
    base_inference->device = g_strdup(DEFAULT_DEVICE);
    base_inference->model_proc = g_strdup(DEFAULT_MODEL_PROC);
    base_inference->batch_size = DEFAULT_BATCH_SIZE;
    base_inference->every_nth_frame = DEFAULT_EVERY_NTH_FRAME;
    base_inference->nireq = DEFAULT_NIREQ;
    base_inference->inference_id = g_strdup(DEFAULT_INFERENCE_ID);
    base_inference->cpu_streams = g_strdup(DEFAULT_CPU_STREAMS);
    base_inference->infer_config = g_strdup("");
    base_inference->allocator_name = g_strdup(DEFAULT_ALLOCATOR_NAME);

    base_inference->initialized = FALSE;
    base_inference->info = NULL;
    base_inference->is_full_frame = TRUE;
    base_inference->inference = NULL;
    base_inference->pre_proc = NULL;
    base_inference->post_proc = NULL;
    base_inference->get_roi_pre_proc = NULL;
}

GstStateChangeReturn gva_base_inference_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GvaBaseInference *base_inference;

    base_inference = GVA_BASE_INFERENCE(element);
    GST_DEBUG_OBJECT(base_inference, "gva_base_inference_change_state");

    ret = GST_ELEMENT_CLASS(gva_base_inference_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gva_base_inference_init(base_inference);
        break;
    }
    default:
        break;
    }

    return ret;
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
        if (model_path != NULL) {
            if (base_inference->model)
                g_free(base_inference->model);
            base_inference->model = g_strdup(model_path);
            GST_INFO("Model : %s", base_inference->model);
        } else {
            g_warning("You cannot change 'model' property on base_inference when a file is open");
        }
    }
}

void gva_base_inference_set_model_proc(GvaBaseInference *base_inference, const gchar *model_proc_path) {
    if (check_gva_base_inference_stopped(base_inference)) {
        if (model_proc_path != NULL) {
            if (base_inference->model_proc)
                g_free(base_inference->model_proc);
            base_inference->model_proc = g_strdup(model_proc_path);
            GST_INFO("Model proc: %s", base_inference->model_proc);
        } else {
            g_warning("You cannot change 'model-proc' property on base_inference when a file is open");
        }
    }
}

void gva_base_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(object);

    GST_DEBUG_OBJECT(base_inference, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gva_base_inference_set_model(base_inference, g_value_get_string(value));
        break;
    case PROP_OBJECT_CLASS:
        base_inference->object_class = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        base_inference->device = g_value_dup_string(value);
        break;
    case PROP_MODEL_PROC:
        gva_base_inference_set_model_proc(base_inference, g_value_get_string(value));
        break;
    case PROP_BATCH_SIZE:
        base_inference->batch_size = g_value_get_uint(value);
        break;
    case PROP_EVERY_NTH_FRAME:
        base_inference->every_nth_frame = g_value_get_uint(value);
        break;
    case PROP_NIREQ:
        base_inference->nireq = g_value_get_uint(value);
        break;
    case PROP_INFERENCE_ID:
        base_inference->inference_id = g_value_dup_string(value);
        break;
    case PROP_CPU_STREAMS:
        base_inference->cpu_streams = g_value_dup_string(value);
        break;
    case PROP_INFER_CONFIG:
        base_inference->infer_config = g_value_dup_string(value);
        break;
    case PROP_ALLOCATOR_NAME:
        base_inference->allocator_name = g_value_dup_string(value);
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
    case PROP_OBJECT_CLASS:
        g_value_set_string(value, base_inference->object_class);
        break;
    case PROP_DEVICE:
        g_value_set_string(value, base_inference->device);
        break;
    case PROP_MODEL_PROC:
        g_value_set_string(value, base_inference->model_proc);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, base_inference->batch_size);
        break;
    case PROP_EVERY_NTH_FRAME:
        g_value_set_uint(value, base_inference->every_nth_frame);
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, base_inference->nireq);
        break;
    case PROP_INFERENCE_ID:
        g_value_set_string(value, base_inference->inference_id);
        break;
    case PROP_CPU_STREAMS:
        g_value_set_string(value, base_inference->cpu_streams);
        break;
    case PROP_INFER_CONFIG:
        g_value_set_string(value, base_inference->infer_config);
        break;
    case PROP_ALLOCATOR_NAME:
        g_value_set_string(value, base_inference->allocator_name);
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

    return TRUE;
}

/* states */
gboolean gva_base_inference_start(GstBaseTransform *trans) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "start");

    if (base_inference->initialized)
        goto exit;

    if (!base_inference->inference_id) {
        base_inference->inference_id = g_strdup(GST_ELEMENT_NAME(GST_ELEMENT(base_inference)));
    }

    GError *error = NULL;
    base_inference->inference = acquire_inference_instance(base_inference, &error);
    if (error) {
        GST_ELEMENT_ERROR(base_inference, RESOURCE, TOO_LAZY, ("base_inference plugin intitialization failed"),
                          ("%s", error->message));
        g_error_free(error);
        goto exit;
    }

    base_inference->initialized = TRUE;

exit:
    return base_inference->initialized;
}

gboolean gva_base_inference_stop(GstBaseTransform *trans) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "stop");
    // FIXME: Hangs when multichannel
    // flush_inference_classify(base_inference);

    return TRUE;
}

gboolean gva_base_inference_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "sink_event");

    classify_inference_sink_event(base_inference, event);

    return GST_BASE_TRANSFORM_CLASS(gva_base_inference_parent_class)->sink_event(trans, event);
}

GstFlowReturn gva_base_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(trans);

    GST_DEBUG_OBJECT(base_inference, "transform_ip");

    if (!base_inference->inference) { // TODO find a way to move this check out to initialization stage
        GST_ELEMENT_ERROR(base_inference, RESOURCE, SETTINGS,
                          ("There is no master element provided for base_inference elements with inference-id '%s'. At "
                           "least one element for each inference-id should have model path specified",
                           base_inference->inference_id),
                          (NULL));
        return GST_FLOW_ERROR;
    }

    return frame_to_classify_inference(base_inference, trans, buf, base_inference->info);

    /* return GST_FLOW_OK; FIXME shouldn't signal about dropping frames in inplace transform function*/
}
