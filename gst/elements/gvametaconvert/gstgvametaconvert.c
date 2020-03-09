/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvametaconvert.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"
#include "converters.h"
#include "gva_caps.h"
#include "gva_json_meta.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Metadata converter"
#define ELEMENT_DESCRIPTION "Metadata converter"

GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_convert_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_convert_debug_category
/* prototypes */

static void gst_gva_meta_convert_set_property(GObject *object, guint property_id, const GValue *value,
                                              GParamSpec *pspec);
static void gst_gva_meta_convert_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_meta_convert_dispose(GObject *object);
static void gst_gva_meta_convert_finalize(GObject *object);

static gboolean gst_gva_meta_convert_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_gva_meta_convert_start(GstBaseTransform *trans);
static gboolean gst_gva_meta_convert_stop(GstBaseTransform *trans);
static gboolean gst_gva_meta_convert_sink_event(GstBaseTransform *trans, GstEvent *event);
static GstFlowReturn gst_gva_meta_convert_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_meta_convert_cleanup(GstGvaMetaConvert *gvametaconvert);
static void gst_gva_meta_convert_reset(GstGvaMetaConvert *gvametaconvert);
static GstStateChangeReturn gst_gva_meta_convert_change_state(GstElement *element, GstStateChange transition);

enum {
    SIGNAL_HANDOFF,
    /* FILL ME */
    LAST_SIGNAL
};
static guint gst_interpret_signals[LAST_SIGNAL] = {0};

#define DEFAULT_MODEL NULL
#define DEFAULT_LAYER_NAME NULL
#define DEFAULT_THRESHOLD 0.5
#define DEFAULT_CONVERTER GST_GVA_METACONVERT_JSON
#define DEFAULT_SIGNAL_HANDOFFS FALSE
#define DEFAULT_INFERENCE_ID NULL
#define DEFAULT_METHOD GST_GVA_METACONVERT_ALL
#define DEFAULT_SOURCE NULL
#define DEFAULT_TAGS NULL
#define DEFAULT_INCLUDE_NO_DETECTIONS FALSE
#define DEFAULT_LOCATION "."

enum {
    PROP_0,
    PROP_CONVERTER,
    PROP_METHOD,
    PROP_MODEL,
    PROP_LAYER_NAME,
    PROP_INFERENCE_ID,
    PROP_SIGNAL_HANDOFFS,
    PROP_SOURCE,
    PROP_TAGS,
    PROP_INCLUDE_NO_DETECTIONS,
    PROP_LOCATION
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaMetaConvert, gst_gva_meta_convert, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_meta_convert_debug_category, "gvametaconvert", 0,
                                                "debug category for gvametaconvert element"));

GType gst_gva_metaconvert_get_converter(void) {
    static GType gva_metaconvert_converter_type = 0;
    static const GEnumValue converter_types[] = {
        {GST_GVA_METACONVERT_TENSOR2TEXT, "Tensor to text conversion", "tensor2text"},
        {GST_GVA_METACONVERT_JSON, "Conversion to GstGVAJSONMeta", "json"},
        {GST_GVA_METACONVERT_TENSORS_TO_FILE, "Tensors to file", "tensors-to-file"},
        {GST_GVA_METACONVERT_DUMP_DETECTION, "Dump detection to GST debug log", "dump-detection"},
        {GST_GVA_METACONVERT_DUMP_CLASSIFICATION, "Dump classification to GST debug log", "dump-classification"},
        {GST_GVA_METACONVERT_DUMP_TENSORS, "Dump tensors to GST debug log", "dump-tensors"},
        {GST_GVA_METACONVERT_ADD_FULL_FRAME_ROI, "Add fullframe ROI", "add-fullframe-roi"},
        {0, NULL, NULL}};

    if (!gva_metaconvert_converter_type) {
        gva_metaconvert_converter_type = g_enum_register_static("GstGVAMetaconvertConverterType", converter_types);
    }
    return gva_metaconvert_converter_type;
}

#define GST_TYPE_GVA_METACONVERT_METHOD (gst_gva_metaconvert_get_method())
static GType gst_gva_metaconvert_get_method(void) {
    static GType gva_metaconvert_method_type = 0;
    static const GEnumValue method_types[] = {{GST_GVA_METACONVERT_ALL, "All conversion", "all"},
                                              {GST_GVA_METACONVERT_DETECTION, "Detection conversion", "detection"},
                                              {GST_GVA_METACONVERT_TENSOR, "Tensor conversion", "tensor"},
                                              {GST_GVA_METACONVERT_MAX, "Max conversion", "max"},
                                              {GST_GVA_METACONVERT_INDEX, "Index conversion", "index"},
                                              {GST_GVA_METACONVERT_COMPOUND, "Compound conversion", "compound"},
                                              {0, NULL, NULL}};

    if (!gva_metaconvert_method_type) {
        gva_metaconvert_method_type = g_enum_register_static("GstGVAMetaconvertMethodType", method_types);
    }
    return gva_metaconvert_method_type;
}

static void gst_gva_metaconvert_set_converter(GstGvaMetaConvert *gvametaconvert,
                                              GstGVAMetaconvertConverterType converter_type) {
    GST_DEBUG_OBJECT(gvametaconvert, "setting converter to %d", converter_type);

    GHashTable *converters = get_converters();
    convert_function_type convert_function = g_hash_table_lookup(converters, GINT_TO_POINTER(converter_type));

    if (convert_function) {
        gvametaconvert->converter = converter_type;
        gvametaconvert->convert_function = convert_function;
    } else
        g_assert_not_reached();

    g_hash_table_destroy(converters);
}

static void gst_gva_meta_convert_class_init(GstGvaMetaConvertClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_meta_convert_set_property;
    gobject_class->get_property = gst_gva_meta_convert_get_property;
    gobject_class->dispose = gst_gva_meta_convert_dispose;
    gobject_class->finalize = gst_gva_meta_convert_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_meta_convert_change_state);

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model",
                                                        "Select/filter tensor to be converted by model name",
                                                        DEFAULT_MODEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_LAYER_NAME,
        g_param_spec_string("layer-name", "Layer Name", "Select/filter tensor to be converted by output layer name",
                            DEFAULT_LAYER_NAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_ID,
        g_param_spec_string("inference-id", "Inference Id",
                            "Select/filter tensor to be converted by GStreamer element name", DEFAULT_INFERENCE_ID,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_CONVERTER,
                                    g_param_spec_enum("converter", "Conversion group", "Conversion group",
                                                      GST_TYPE_GVA_METACONVERT_CONVERTER, DEFAULT_CONVERTER,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_METHOD,
                                    g_param_spec_enum("method", "Conversion method", "Conversion method",
                                                      GST_TYPE_GVA_METACONVERT_METHOD, DEFAULT_METHOD,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_LOCATION,
                                    g_param_spec_string("location", "Location for the output files", "Path to folder",
                                                        DEFAULT_LOCATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(
        gobject_class, PROP_SIGNAL_HANDOFFS,
        g_param_spec_boolean("signal-handoffs", "Signal handoffs", "Send signal before pushing the buffer",
                             DEFAULT_SIGNAL_HANDOFFS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_SOURCE,
                                    g_param_spec_string("source", "Source URI", "Source URI", DEFAULT_SOURCE,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_TAGS,
                                    g_param_spec_string("tags", "Custom tags",
                                                        "JSON object of custom values added to json message",
                                                        DEFAULT_TAGS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_INCLUDE_NO_DETECTIONS,
                                    g_param_spec_boolean("include-no-detections", "include metas with no detections",
                                                         "Convert a JSON message with no detections",
                                                         DEFAULT_INCLUDE_NO_DETECTIONS,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_interpret_signals[SIGNAL_HANDOFF] = g_signal_new(
        "handoff", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(GstGvaMetaConvertClass, handoff), NULL,
        NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 1, GST_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void gst_gva_meta_convert_cleanup(GstGvaMetaConvert *gvametaconvert) {
    if (gvametaconvert == NULL)
        return;

    GST_DEBUG_OBJECT(gvametaconvert, "gst_gva_meta_convert_cleanup");

    g_free(gvametaconvert->inference_id);
    g_free(gvametaconvert->layer_name);
    g_free(gvametaconvert->source);
    g_free(gvametaconvert->tags);
    g_free(gvametaconvert->location);
    if (gvametaconvert->info)
        gst_video_info_free(gvametaconvert->info);
}

static void gst_gva_meta_convert_reset(GstGvaMetaConvert *gvametaconvert) {
    GST_DEBUG_OBJECT(gvametaconvert, "gst_gva_meta_convert_reset");

    if (gvametaconvert == NULL)
        return;

    gst_gva_meta_convert_cleanup(gvametaconvert);

    gvametaconvert->model = g_strdup(DEFAULT_MODEL);
    gvametaconvert->layer_name = g_strdup(DEFAULT_LAYER_NAME);
    gvametaconvert->inference_id = g_strdup(DEFAULT_INFERENCE_ID);
    gvametaconvert->method = DEFAULT_METHOD;
    gvametaconvert->source = g_strdup(DEFAULT_SOURCE);
    gvametaconvert->tags = g_strdup(DEFAULT_TAGS);
    gvametaconvert->include_no_detections = DEFAULT_INCLUDE_NO_DETECTIONS;
    gvametaconvert->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;
    gvametaconvert->threshold = DEFAULT_THRESHOLD;
    gst_gva_metaconvert_set_converter(gvametaconvert, DEFAULT_CONVERTER);
    gvametaconvert->location = g_strdup(DEFAULT_LOCATION);
    gvametaconvert->info = NULL;
}

static GstStateChangeReturn gst_gva_meta_convert_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaMetaConvert *gvametaconvert;

    gvametaconvert = GST_GVA_META_CONVERT(element);
    GST_DEBUG_OBJECT(gvametaconvert, "gst_gva_meta_convert_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_meta_convert_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_meta_convert_reset(gvametaconvert);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_meta_convert_init(GstGvaMetaConvert *gvametaconvert) {
    gst_gva_meta_convert_reset(gvametaconvert);
}

void gst_gva_meta_convert_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(object);

    GST_DEBUG_OBJECT(gvametaconvert, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gvametaconvert->model = g_value_dup_string(value);
        break;
    case PROP_LAYER_NAME:
        gvametaconvert->layer_name = g_value_dup_string(value);
        break;
    case PROP_INFERENCE_ID:
        gvametaconvert->inference_id = g_value_dup_string(value);
        break;
    case PROP_CONVERTER:
        gst_gva_metaconvert_set_converter(gvametaconvert, g_value_get_enum(value));
        break;
    case PROP_METHOD:
        gvametaconvert->method = g_value_get_enum(value);
        break;
    case PROP_SOURCE:
        gvametaconvert->source = g_value_dup_string(value);
        break;
    case PROP_TAGS:
        gvametaconvert->tags = g_value_dup_string(value);
        break;
    case PROP_INCLUDE_NO_DETECTIONS:
        gvametaconvert->include_no_detections = g_value_get_boolean(value);
        break;
    case PROP_SIGNAL_HANDOFFS:
        gvametaconvert->signal_handoffs = g_value_get_boolean(value);
        break;
    case PROP_LOCATION:
        gvametaconvert->location = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_meta_convert_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(object);

    GST_DEBUG_OBJECT(gvametaconvert, "get_property");

    switch (property_id) {
    case PROP_INFERENCE_ID:
        g_value_set_string(value, gvametaconvert->inference_id);
        break;
    case PROP_MODEL:
        g_value_set_string(value, gvametaconvert->model);
        break;
    case PROP_LAYER_NAME:
        g_value_set_string(value, gvametaconvert->layer_name);
        break;
    case PROP_CONVERTER:
        g_value_set_enum(value, gvametaconvert->converter);
        break;
    case PROP_METHOD:
        g_value_set_enum(value, gvametaconvert->method);
        break;
    case PROP_SOURCE:
        g_value_set_string(value, gvametaconvert->source);
        break;
    case PROP_TAGS:
        g_value_set_string(value, gvametaconvert->tags);
        break;
    case PROP_INCLUDE_NO_DETECTIONS:
        g_value_set_boolean(value, gvametaconvert->include_no_detections);
        break;
    case PROP_SIGNAL_HANDOFFS:
        g_value_set_boolean(value, gvametaconvert->signal_handoffs);
        break;
    case PROP_LOCATION:
        g_value_set_string(value, gvametaconvert->location);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_meta_convert_dispose(GObject *object) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(object);

    GST_DEBUG_OBJECT(gvametaconvert, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_meta_convert_parent_class)->dispose(object);
}

void gst_gva_meta_convert_finalize(GObject *object) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(object);

    GST_DEBUG_OBJECT(gvametaconvert, "finalize");

    /* clean up object here */

    gst_gva_meta_convert_cleanup(gvametaconvert);

    G_OBJECT_CLASS(gst_gva_meta_convert_parent_class)->finalize(object);
}

static gboolean gst_gva_meta_convert_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(trans);
    GST_DEBUG_OBJECT(gvametaconvert, "set_caps");
    if (!gvametaconvert->info) {
        gvametaconvert->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gvametaconvert->info, incaps);
    return TRUE;
}

/* states */
static gboolean gst_gva_meta_convert_start(GstBaseTransform *trans) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(trans);

    GST_DEBUG_OBJECT(gvametaconvert, "start");

    return TRUE;
}

static gboolean gst_gva_meta_convert_stop(GstBaseTransform *trans) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(trans);

    GST_DEBUG_OBJECT(gvametaconvert, "stop");

    return TRUE;
}

/* sink and src pad event handlers */
static gboolean gst_gva_meta_convert_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(trans);

    GST_DEBUG_OBJECT(gvametaconvert, "sink_event");

    return GST_BASE_TRANSFORM_CLASS(gst_gva_meta_convert_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_meta_convert_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaMetaConvert *gvametaconvert = GST_GVA_META_CONVERT(trans);
    GstFlowReturn status = GST_FLOW_OK;

    if (gvametaconvert->signal_handoffs) {
        g_signal_emit(gvametaconvert, gst_interpret_signals[SIGNAL_HANDOFF], 0, buf);
    } else if (gvametaconvert->convert_function) {
        status = gvametaconvert->convert_function(gvametaconvert, buf) ? GST_FLOW_OK : GST_FLOW_ERROR;
    }

    return status;
}
