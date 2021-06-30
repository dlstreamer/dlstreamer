/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvaactionrecognitionbin.hpp"

#include <gva_caps.h>

#include <stdexcept>

GST_DEBUG_CATEGORY(gst_gva_action_recognition_bin_debug_category);

static constexpr guint DEFAULT_MIN_NIREQ = 1;
static constexpr guint DEFAULT_MAX_NIREQ = 1024;
static constexpr guint DEFAULT_NIREQ = DEFAULT_MIN_NIREQ;

static constexpr guint DEFAULT_MIN_BATCH_SIZE = 1;
static constexpr guint DEFAULT_MAX_BATCH_SIZE = 1024;
static constexpr guint DEFAULT_BATCH_SIZE = DEFAULT_MIN_BATCH_SIZE;

static constexpr auto DEFAULT_DEVICE = "CPU";

/* Properties */
enum {
    PROP_0,
    /* encoder */
    PROP_ENC_MODEL,
    PROP_ENC_IE_CONFIG,
    PROP_ENC_DEVICE,
    PROP_ENC_NIREQ,
    PROP_ENC_BATCH_SIZE,
    /* decoder */
    PROP_DEC_MODEL,
    PROP_DEC_IE_CONFIG,
    PROP_DEC_DEVICE,
    PROP_DEC_NIREQ,
    PROP_DEC_BATCH_SIZE,
    /* pre-post-proc */
    PROP_MODEL_PROC,
    PROP_PRE_PROC_BACKEND
};

/* TODO: use enum from gvavideototensor */
#define GST_TYPE_GVA_ACTION_RECOGNITION_BACKEND (gst_gva_action_recignition_bin_backend_get_type())

static GType gst_gva_action_recignition_bin_backend_get_type(void) {
    static GType gva_action_recignition_bin_backend_type = 0;
    static const GEnumValue backend_types[] = {
        {OPENCV, "OpenCV", "opencv"},
        {IE, "Inference Engine", "ie"},
#ifdef ENABLE_VAAPI
        {VAAPI_SYSTEM, "VAAPI", "vaapi"},
        {VAAPI_SURFACE_SHARING, "VAAPI Surface Sharing", "vaapi-surface-sharing"},
#endif
        {0, NULL, NULL}};

    if (!gva_action_recignition_bin_backend_type) {
        gva_action_recignition_bin_backend_type =
            g_enum_register_static("GstGVAActionRecognitionBinBackend", backend_types);
    }

    return gva_action_recignition_bin_backend_type;
}

static constexpr auto DEFAULT_PRE_PROC_BACKEND = OPENCV;

#define gst_gva_action_recognition_bin_parent_class parent_class
G_DEFINE_TYPE(GstGvaActionRecognitionBin, gst_gva_action_recognition_bin, GST_TYPE_BIN);

static void gst_gva_action_recognition_bin_finalize(GObject *object);
static void gst_gva_action_recognition_bin_get_property(GObject *object, guint prop_id, GValue *value,
                                                        GParamSpec *pspec);
static void gst_gva_action_recognition_bin_set_property(GObject *object, guint prop_id, const GValue *value,
                                                        GParamSpec *pspec);

static GstStateChangeReturn gst_gva_action_recognition_bin_change_state(GstElement *element, GstStateChange transition);

static void gst_gva_action_recognition_bin_class_init(GstGvaActionRecognitionBinClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    element_class->change_state = gst_gva_action_recognition_bin_change_state;

    gobject_class->set_property = gst_gva_action_recognition_bin_set_property;
    gobject_class->get_property = gst_gva_action_recognition_bin_get_property;
    gobject_class->finalize = gst_gva_action_recognition_bin_finalize;

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    /* encoder properties */
    g_object_class_install_property(gobject_class, PROP_ENC_MODEL,
                                    g_param_spec_string("enc-model", "Encoder Model",
                                                        "Path to encoder inference model network file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_ENC_IE_CONFIG,
        g_param_spec_string("enc-ie-config", "Encoder Inference-Engine-Config",
                            "Encoder's comma separated list of KEY=VALUE parameters for Inference Engine configuration",
                            NULL, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_ENC_DEVICE,
                                    g_param_spec_string("enc-device", "Encoder Device",
                                                        "Encoder inference device: [CPU, GPU]", DEFAULT_DEVICE,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_ENC_NIREQ,
        g_param_spec_uint("enc-nireq", "Encoder NIReq", "Encoder's number of inference requests", DEFAULT_MIN_NIREQ,
                          DEFAULT_MAX_NIREQ, DEFAULT_NIREQ, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_ENC_BATCH_SIZE,
        g_param_spec_uint(
            "enc-batch-size", "Encoder Batch Size",
            "Number of frames batched together for a single encoder inference. Not all models support batching. "
            "Use model optimizer to ensure that the model has batching support.",
            DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    /* decoder properties */
    g_object_class_install_property(gobject_class, PROP_DEC_MODEL,
                                    g_param_spec_string("dec-model", "Decoder Model",
                                                        "Path to decoder inference model network file", NULL,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_DEC_IE_CONFIG,
        g_param_spec_string("dec-ie-config", "Decoder Inference-Engine-Config",
                            "Decoder's comma separated list of KEY=VALUE parameters for Inference Engine configuration",
                            NULL, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_DEC_DEVICE,
                                    g_param_spec_string("dec-device", "Decoder Device",
                                                        "Decoder inference device: [CPU, GPU]", DEFAULT_DEVICE,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_DEC_NIREQ,
        g_param_spec_uint("dec-nireq", "Decoder NIReq", "Decoder's number of inference requests", DEFAULT_MIN_NIREQ,
                          DEFAULT_MAX_NIREQ, DEFAULT_NIREQ, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        gobject_class, PROP_DEC_BATCH_SIZE,
        g_param_spec_uint(
            "dec-batch-size", "Decoder Batch Size",
            "Number of frames batched together for a single decoder inference. Not all models support batching. "
            "Use model optimizer to ensure that the model has batching support.",
            DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    /* pre-post-proc properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "Path to JSON file with description of input/output layers pre-processing/post-processing",
                            nullptr, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_PRE_PROC_BACKEND,
                                    g_param_spec_enum("pre-proc-backend", "Preproc Backend",
                                                      "Preprocessing backend type",
                                                      GST_TYPE_GVA_ACTION_RECOGNITION_BACKEND, DEFAULT_PRE_PROC_BACKEND,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_set_metadata(element_class, GST_GVA_ACTION_RECOGNITION_BIN_NAME, "video",
                                   GST_GVA_ACTION_RECOGNITION_BIN_DESCRIPTION, "Intel Corporation");
}

static void gst_gva_action_recognition_bin_init(GstGvaActionRecognitionBin *self) {
    // Initialize C++ structure with new
    new (&self->props) GstGvaActionRecognitionBin::_Props();

    self->props.enc_nireq = DEFAULT_NIREQ;
    self->props.dec_nireq = DEFAULT_NIREQ;

    self->tee = gst_element_factory_make("tee", nullptr);
    self->queue1 = gst_element_factory_make("queue", nullptr);
    self->queue2 = gst_element_factory_make("queue", nullptr);
    self->preproc = gst_element_factory_make("gvavideototensor", nullptr);
    self->encoder_inference = gst_element_factory_make("gvatensorinference", nullptr);
    self->acc = gst_element_factory_make("gvatensoracc", nullptr);
    self->decoder_inference = gst_element_factory_make("gvatensorinference", nullptr);
    self->postproc = gst_element_factory_make("gvatensortometa", nullptr);
    self->aggregate = gst_element_factory_make("gvametaaggregate", nullptr);

    gst_bin_add_many(GST_BIN(self), self->tee, self->queue1, self->queue2, self->preproc, self->encoder_inference,
                     self->acc, self->decoder_inference, self->postproc, self->aggregate, nullptr);

    GstPad *pad = gst_element_get_static_pad(self->aggregate, "src");
    if (pad) {
        GST_DEBUG_OBJECT(self, "setting target src pad %" GST_PTR_FORMAT, pad);
        self->srcpad = gst_ghost_pad_new("src", pad);
        gst_element_add_pad(GST_ELEMENT_CAST(self), self->srcpad);
        gst_object_unref(pad);
    }

    pad = gst_element_get_static_pad(self->tee, "sink");
    if (pad) {
        GST_DEBUG_OBJECT(self, "setting target sink pad %" GST_PTR_FORMAT, pad);
        self->sinkpad = gst_ghost_pad_new("sink", pad);
        gst_element_add_pad(GST_ELEMENT_CAST(self), self->sinkpad);
        gst_object_unref(pad);
    }
}

static void gst_gva_action_recognition_bin_finalize(GObject *object) {
    GstGvaActionRecognitionBin *self = GST_GVA_ACTION_RECOGNITION_BIN(object);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

void gst_gva_action_recognition_bin_finish_init(GstGvaActionRecognitionBin * /* self */) {
}

static void gst_gva_action_recognition_bin_get_property(GObject *object, guint prop_id, GValue *value,
                                                        GParamSpec *pspec) {
    GstGvaActionRecognitionBin *self = GST_GVA_ACTION_RECOGNITION_BIN(object);

    switch (prop_id) {
    case PROP_ENC_MODEL:
        g_value_set_string(value, self->props.enc_model.c_str());
        break;
    case PROP_ENC_IE_CONFIG:
        g_value_set_string(value, self->props.enc_ie_config.c_str());
        break;
    case PROP_ENC_DEVICE:
        g_value_set_string(value, self->props.enc_device.c_str());
        break;
    case PROP_ENC_NIREQ:
        g_value_set_uint(value, self->props.enc_nireq);
        break;
    case PROP_ENC_BATCH_SIZE:
        g_value_set_uint(value, self->props.enc_batch_size);
        break;
    case PROP_DEC_MODEL:
        g_value_set_string(value, self->props.dec_model.c_str());
        break;
    case PROP_DEC_IE_CONFIG:
        g_value_set_string(value, self->props.dec_ie_config.c_str());
        break;
    case PROP_DEC_DEVICE:
        g_value_set_string(value, self->props.dec_device.c_str());
        break;
    case PROP_DEC_NIREQ:
        g_value_set_uint(value, self->props.dec_nireq);
        break;
    case PROP_DEC_BATCH_SIZE:
        g_value_set_uint(value, self->props.dec_batch_size);
        break;
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

static void gst_gva_action_recognition_bin_set_property(GObject *object, guint prop_id, const GValue *value,
                                                        GParamSpec *pspec) {
    GstGvaActionRecognitionBin *self = GST_GVA_ACTION_RECOGNITION_BIN(object);

    switch (prop_id) {
    case PROP_ENC_MODEL:
        self->props.enc_model = g_value_get_string(value);
        GST_INFO_OBJECT(self, "encoder model property set: %s", self->props.enc_model.c_str());
        g_object_set(G_OBJECT(self->encoder_inference), "model", self->props.enc_model.c_str(), nullptr);
        break;

    case PROP_ENC_IE_CONFIG:
        self->props.enc_ie_config = g_value_get_string(value);
        GST_INFO_OBJECT(self, "encoder ie-config property set: %s", self->props.enc_ie_config.c_str());
        g_object_set(G_OBJECT(self->encoder_inference), "ie-config", self->props.enc_ie_config.c_str(), nullptr);
        break;

    case PROP_ENC_DEVICE:
        self->props.enc_device = g_value_get_string(value);
        GST_INFO_OBJECT(self, "encoder device property set: %s", self->props.enc_device.c_str());
        g_object_set(G_OBJECT(self->encoder_inference), "device", self->props.enc_device.c_str(), nullptr);
        break;

    case PROP_ENC_NIREQ:
        self->props.enc_nireq = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "encoder nireq property set: %u", self->props.enc_nireq);
        g_object_set(G_OBJECT(self->encoder_inference), "nireq", self->props.enc_nireq, nullptr);
        break;

    case PROP_ENC_BATCH_SIZE:
        self->props.enc_batch_size = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "encoder batch-size property set: %u", self->props.enc_batch_size);
        g_object_set(G_OBJECT(self->encoder_inference), "batch-size", self->props.enc_batch_size, nullptr);
        break;

    case PROP_DEC_MODEL:
        self->props.dec_model = g_value_get_string(value);
        GST_INFO_OBJECT(self, "decoder model property set: %s", self->props.dec_model.c_str());
        g_object_set(G_OBJECT(self->decoder_inference), "model", self->props.dec_model.c_str(), nullptr);
        break;

    case PROP_DEC_IE_CONFIG:
        self->props.dec_ie_config = g_value_get_string(value);
        GST_INFO_OBJECT(self, "decoder ie-config property set: %s", self->props.dec_ie_config.c_str());
        g_object_set(G_OBJECT(self->decoder_inference), "ie-config", self->props.dec_ie_config.c_str(), nullptr);
        break;

    case PROP_DEC_DEVICE:
        self->props.dec_device = g_value_get_string(value);
        if (self->props.dec_device != "CPU") {
            GST_ERROR_OBJECT(self, "Decoder device can only be equal to \'CPU\'.");
            throw std::logic_error("Not implemented yet.");
        }
        GST_INFO_OBJECT(self, "decoder device property set: %s", self->props.dec_device.c_str());
        g_object_set(G_OBJECT(self->decoder_inference), "device", self->props.dec_device.c_str(), nullptr);
        break;

    case PROP_DEC_NIREQ:
        self->props.dec_nireq = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "decoder nireq property set: %u", self->props.dec_nireq);
        g_object_set(G_OBJECT(self->decoder_inference), "nireq", self->props.dec_nireq, nullptr);
        break;

    case PROP_DEC_BATCH_SIZE:
        self->props.dec_batch_size = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "decoder batch-size property set: %u", self->props.dec_batch_size);
        g_object_set(G_OBJECT(self->decoder_inference), "batch-size", self->props.dec_batch_size, nullptr);
        break;

    case PROP_MODEL_PROC:
        self->props.model_proc = g_value_get_string(value);
        g_object_set(G_OBJECT(self->postproc), "model-proc", self->props.model_proc.c_str(), nullptr);
        g_object_set(G_OBJECT(self->preproc), "model-proc", self->props.model_proc.c_str(), nullptr);
        GST_INFO_OBJECT(self, "model-proc property set: %s", self->props.model_proc.c_str());
        break;

    case PROP_PRE_PROC_BACKEND:
        self->props.pre_proc_backend = static_cast<PreProcBackend>(g_value_get_enum(value));
        g_object_set(G_OBJECT(self->preproc), "pre-proc-backend", self->props.pre_proc_backend, nullptr);
        // pre-proc-backend
        // GST_INFO_OBJECT(self, "pre-proc-backend property set: %s", self->props.pre_proc_backend);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void link_elements(GstGvaActionRecognitionBin *self) {
    // tee[0] -> queue1 -> aggregate[0]
    auto teeAggregatePad = gst_element_get_request_pad(self->tee, "src_%u");
    auto aggTeePad = gst_element_get_request_pad(self->aggregate, "sink_%u");
    gst_pad_link(teeAggregatePad, gst_element_get_static_pad(self->queue1, "sink"));
    gst_pad_link(gst_element_get_static_pad(self->queue1, "src"), aggTeePad);

    // tee[1] -> queue2
    auto teePreProcPad = gst_element_get_request_pad(self->tee, "src_%u");
    gst_pad_link(teePreProcPad, gst_element_get_static_pad(self->queue2, "sink"));

    // queue2 -> preproc
    gst_element_link_pads(self->queue2, "src", self->preproc, "sink");

    // preproc -> enc_infer
    gst_element_link_pads(self->preproc, "src", self->encoder_inference, "sink");

    // enc_infer -> acc
    gst_element_link_pads(self->encoder_inference, "src", self->acc, "sink");

    // acc -> dec_infer
    gst_element_link_pads(self->acc, "src", self->decoder_inference, "sink");

    // dec_infer -> postproc
    gst_element_link_pads(self->decoder_inference, "src", self->postproc, "sink");

    // postproc -> aggregate[1]
    auto aggInferPad = gst_element_get_request_pad(self->aggregate, "sink_%u");
    gst_pad_link(gst_element_get_static_pad(self->postproc, "src"), aggInferPad);
}

static GstStateChangeReturn gst_gva_action_recognition_bin_change_state(GstElement *element,
                                                                        GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaActionRecognitionBin *self = GST_GVA_ACTION_RECOGNITION_BIN(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        link_elements(self);
        break;
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    switch (transition) {
    default:
        break;
    }

    return ret;
}
