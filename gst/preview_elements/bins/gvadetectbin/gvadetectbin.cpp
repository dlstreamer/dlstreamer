/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvadetectbin.hpp"

#include <gva_caps.h>
#include <post_processor/post_proc_common.h>

#include <stdexcept>

GST_DEBUG_CATEGORY(gva_detect_bin_debug_category);

namespace {
constexpr guint MIN_NIREQ = 0;
constexpr guint MAX_NIREQ = 1024;
constexpr guint DEFAULT_NIREQ = MIN_NIREQ;
constexpr guint MIN_BATCH_SIZE = 1;
constexpr guint MAX_BATCH_SIZE = 1024;
constexpr guint DEFAULT_BATCH_SIZE = MIN_BATCH_SIZE;
constexpr guint MIN_INTERVAL = 1;
constexpr guint MAX_INTERVAL = std::numeric_limits<guint>::max();
constexpr guint DEFAULT_INTERVAL = 1;
} // namespace

/* Properties */
enum {
    PROP_0,
    /* inference */
    PROP_MODEL,
    PROP_IE_CONFIG,
    PROP_DEVICE,
    PROP_INSTANCE_ID,
    PROP_NIREQ,
    PROP_BATCH_SIZE,
    /* pre-post-proc */
    PROP_MODEL_PROC,
    PROP_PRE_PROC_BACKEND,
    PROP_INTERVAL,
    PROP_CONVERTER_TYPE,
    PROP_ROI_LIST
};

#define gva_detect_bin_parent_class parent_class
G_DEFINE_TYPE(GvaDetectBin, gva_detect_bin, GST_TYPE_BIN);

static void gva_detect_bin_finalize(GObject *object);
static void gva_detect_bin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gva_detect_bin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

static GstStateChangeReturn gva_detect_bin_change_state(GstElement *element, GstStateChange transition);

static void gva_detect_bin_class_init(GvaDetectBinClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    element_class->change_state = gva_detect_bin_change_state;

    gobject_class->set_property = gva_detect_bin_set_property;
    gobject_class->get_property = gva_detect_bin_get_property;
    gobject_class->finalize = gva_detect_bin_finalize;

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    /* inference properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL,
        g_param_spec_string("model", "Model", "Path to inference model network file", "", prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", "",
                            prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string("device", "Device", "Inference device: [CPU, GPU]", "CPU", prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_INSTANCE_ID,
        g_param_spec_string(
            "instance-id", "Instance ID",
            "Identifier for sharing resources between inference elements of the same type. Elements with "
            "the instance-id will share model and other properties",
            "", prm_flags));
    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests", MIN_NIREQ,
                                                      MAX_NIREQ, DEFAULT_NIREQ, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_BATCH_SIZE,
        g_param_spec_uint("batch-size", "Batch Size",
                          "Number of frames batched together for a single inference. Not all models support batching. "
                          "Use model optimizer to ensure that the model has batching support.",
                          MIN_BATCH_SIZE, MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE, prm_flags));
    /* pre-post-proc properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "Path to JSON file with description of input/output layers pre-processing/post-processing",
                            "", prm_flags));
    g_object_class_install_property(gobject_class, PROP_PRE_PROC_BACKEND,
                                    g_param_spec_enum("pre-process-backend", "Preproc Backend",
                                                      "Preprocessing backend type",
                                                      GST_TYPE_GVA_VIDEO_TO_TENSOR_BACKEND, IE, prm_flags));
    g_object_class_install_property(gobject_class, PROP_INTERVAL,
                                    g_param_spec_uint("inference-interval", "Inference Interval",
                                                      "Run inference for every Nth buffer", MIN_INTERVAL, MAX_INTERVAL,
                                                      DEFAULT_INTERVAL, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_CONVERTER_TYPE,
        g_param_spec_enum("converter-type", "Converter Type", "Postprocessing converter type",
                          GST_TYPE_GVA_TENSOR_TO_META_CONVERTER_TYPE,
                          static_cast<int>(post_processing::ConverterType::TO_ROI), prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_ROI_LIST,
        g_param_spec_boolean("roi-list", "ROI List", "Inference on ROI list", false, prm_flags));

    gst_element_class_set_metadata(element_class, GVA_DETECT_BIN_NAME, "video", GVA_DETECT_BIN_DESCRIPTION,
                                   "Intel Corporation");
}

static void gva_detect_bin_init(GvaDetectBin *self) {
    // Initialize C++ structure with new
    new (&self->props) GvaDetectBin::_Props();

    self->props.nireq = DEFAULT_NIREQ;

    self->tee = gst_element_factory_make("tee", nullptr);
    self->queue1 = gst_element_factory_make("queue", nullptr);
    self->queue2 = gst_element_factory_make("queue", nullptr);
    self->preproc = gst_element_factory_make("gvavideototensor", nullptr);
    self->inference = gst_element_factory_make("gvatensorinference", nullptr);
    self->postproc = gst_element_factory_make("gvatensortometa", nullptr);
    self->aggregate = gst_element_factory_make("tensormux", nullptr);

    gst_bin_add_many(GST_BIN(self), self->tee, self->queue1, self->queue2, self->preproc, self->inference,
                     self->postproc, self->aggregate, nullptr);

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

static void gva_detect_bin_finalize(GObject *object) {
    GvaDetectBin *self = GVA_DETECT_BIN(object);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

void gva_detect_bin_finish_init(GvaDetectBin * /* self */) {
}

static void gva_detect_bin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaDetectBin *self = GVA_DETECT_BIN(object);

    switch (prop_id) {
    case PROP_MODEL:
        g_value_set_string(value, self->props.model.c_str());
        break;
    case PROP_IE_CONFIG:
        g_value_set_string(value, self->props.ie_config.c_str());
        break;
    case PROP_DEVICE:
        g_value_set_string(value, self->props.device.c_str());
        break;
    case PROP_INSTANCE_ID:
        g_value_set_string(value, self->props.instance_id.c_str());
        break;
    case PROP_NIREQ:
        g_value_set_uint(value, self->props.nireq);
        break;
    case PROP_BATCH_SIZE:
        g_value_set_uint(value, self->props.batch_size);
        break;
    case PROP_MODEL_PROC:
        g_value_set_string(value, self->props.model_proc.c_str());
        break;
    case PROP_PRE_PROC_BACKEND:
        g_value_set_enum(value, self->props.pre_proc_backend);
        break;
    case PROP_INTERVAL:
        g_value_set_uint(value, self->props.interval);
        break;
    case PROP_CONVERTER_TYPE:
        g_value_set_enum(value, static_cast<gint>(self->props.converter_type));
        break;
    case PROP_ROI_LIST:
        g_value_set_boolean(value, self->props.roi_list);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_detect_bin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaDetectBin *self = GVA_DETECT_BIN(object);
    switch (prop_id) {
    case PROP_MODEL:
        self->props.model = g_value_get_string(value);
        GST_INFO_OBJECT(self, "tensorinference model property set: %s", self->props.model.c_str());
        g_object_set(G_OBJECT(self->inference), "model", self->props.model.c_str(), nullptr);
        break;
    case PROP_IE_CONFIG:
        self->props.ie_config = g_value_get_string(value);
        GST_INFO_OBJECT(self, "tensorinference ie-config property set: %s", self->props.ie_config.c_str());
        g_object_set(G_OBJECT(self->inference), "ie-config", self->props.ie_config.c_str(), nullptr);
        break;
    case PROP_DEVICE:
        self->props.device = g_value_get_string(value);
        GST_INFO_OBJECT(self, "tensorinference device property set: %s", self->props.device.c_str());
        g_object_set(G_OBJECT(self->inference), "device", self->props.device.c_str(), nullptr);
        break;
    case PROP_INSTANCE_ID:
        self->props.instance_id = g_value_get_string(value);
        GST_INFO_OBJECT(self, "tensorinference instance-id property set: %s", self->props.instance_id.c_str());
        g_object_set(G_OBJECT(self->inference), "instance_id", self->props.instance_id.c_str(), nullptr);
        break;
    case PROP_NIREQ:
        self->props.nireq = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "tensorinference nireq property set: %u", self->props.nireq);
        g_object_set(G_OBJECT(self->inference), "nireq", self->props.nireq, nullptr);
        break;
    case PROP_BATCH_SIZE:
        self->props.batch_size = g_value_get_uint(value);
        GST_INFO_OBJECT(self, "tensorinference batch-size property set: %u", self->props.batch_size);
        g_object_set(G_OBJECT(self->inference), "batch-size", self->props.batch_size, nullptr);
        break;
    case PROP_MODEL_PROC:
        self->props.model_proc = g_value_get_string(value);
        g_object_set(G_OBJECT(self->postproc), "model-proc", self->props.model_proc.c_str(), nullptr);
        g_object_set(G_OBJECT(self->preproc), "model-proc", self->props.model_proc.c_str(), nullptr);
        GST_INFO_OBJECT(self, "model-proc property set: %s", self->props.model_proc.c_str());
        break;
    case PROP_PRE_PROC_BACKEND:
        self->props.pre_proc_backend = static_cast<PreProcBackend>(g_value_get_enum(value));
        g_object_set(G_OBJECT(self->preproc), "pre-process-backend", self->props.pre_proc_backend, nullptr);
        break;
    case PROP_INTERVAL:
        self->props.interval = g_value_get_uint(value);
        g_object_set(G_OBJECT(self->preproc), "interval", self->props.interval, nullptr);
        break;
    case PROP_CONVERTER_TYPE:
        self->props.converter_type = static_cast<post_processing::ConverterType>(g_value_get_enum(value));
        g_object_set(G_OBJECT(self->postproc), "converter-type", self->props.converter_type, nullptr);
        break;
    case PROP_ROI_LIST:
        self->props.roi_list = g_value_get_boolean(value);
        g_object_set(G_OBJECT(self->preproc), "produce-rois", self->props.roi_list, nullptr);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static bool link_elements(GvaDetectBin *self) {
    // tee[0] -> queue2 -> preproc -> inference -> postproc
    if (!gst_element_link_many(self->tee, self->queue2, self->preproc, self->inference, self->postproc, nullptr))
        return false;
    if (!gst_element_link_pads(self->postproc, "src", self->aggregate, "tensor_%u"))
        return false;

    // tee[1] -> queue1 -> aggregate[1]
    if (!gst_element_link_many(self->tee, self->queue1, self->aggregate, nullptr))
        return false;

    return true;
}

static GstStateChangeReturn gva_detect_bin_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GvaDetectBin *self = GVA_DETECT_BIN(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!link_elements(self))
            return GST_STATE_CHANGE_FAILURE;
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
