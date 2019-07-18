/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvametapublish.h"
#include "metapublish_impl.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_publish_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_publish_debug_category

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Generic metadata publisher"
#define ELEMENT_DESCRIPTION "Generic metadata publisher"

/* prototypes */

static void gst_gva_meta_publish_set_property(GObject *object, guint property_id, const GValue *value,
                                              GParamSpec *pspec);
static void gst_gva_meta_publish_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_meta_publish_dispose(GObject *object);
static void gst_gva_meta_publish_finalize(GObject *object);

static gboolean gst_gva_meta_publish_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);

static gboolean gst_gva_meta_publish_start(GstBaseTransform *trans);
static gboolean gst_gva_meta_publish_stop(GstBaseTransform *trans);
static gboolean gst_gva_meta_publish_sink_event(GstBaseTransform *trans, GstEvent *event);

static void gst_gva_meta_publish_before_transform(GstBaseTransform *trans, GstBuffer *buffer);
static GstFlowReturn gst_gva_meta_publish_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_meta_publish_cleanup(GstGvaMetaPublish *gvametapublish);
static void gst_gva_meta_publish_reset(GstGvaMetaPublish *gvametapublish);

enum {
    SIGNAL_HANDOFF,
    /* FILL ME */
    LAST_SIGNAL
};

static guint gst_interpret_signals[LAST_SIGNAL] = {0};

// File specific constants
#define DEFAULT_PUBLISH_METHOD GST_GVA_METAPUBLISH_FILE
#define DEFAULT_FILE_PATH NULL
#define DEFAULT_OUTPUT_FORMAT BATCH

// Broker specific constants
#define DEFAULT_HOST NULL
#define DEFAULT_ADDRESS NULL
#define DEFAULT_CLIENTID NULL
#define DEFAULT_TOPIC NULL
#define DEFAULT_SIGNAL_HANDOFFS FALSE
#define DEFAULT_TIMEOUT NULL

enum {
    PROP_0,
    PROP_PUBLISH_METHOD,
    PROP_FILE_PATH,
    PROP_OUTPUT_FORMAT,
    PROP_HOST,
    PROP_ADDRESS,
    PROP_CLIENTID,
    PROP_TOPIC,
    PROP_TIMEOUT,
    PROP_SIGNAL_HANDOFFS,
};

/* pad templates */

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ NV12 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define VA_SURFACE_CAPS

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")

#define INFERENCE_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS INFERENCE_CAPS
#define VIDEO_SRC_CAPS INFERENCE_CAPS

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaMetaPublish, gst_gva_meta_publish, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_meta_publish_debug_category, "gvametapublish", 0,
                                                "debug category for gvametapublish element"));

#define GST_TYPE_GVA_METAPUBLISH_METHOD (gst_gva_metapublish_get_method())
static GType gst_gva_metapublish_get_method(void) {
    static GType gva_metapublish_method_type = 0;
    static const GEnumValue method_types[] = {{GST_GVA_METAPUBLISH_FILE, "File publish", "file"},
#ifdef PAHO_INC
                                              {GST_GVA_METAPUBLISH_MQTT, "MQTT publish", "mqtt"},
#endif
#ifdef KAFKA_INC
                                              {GST_GVA_METAPUBLISH_KAFKA, "Kafka publish", "kafka"},
#endif
                                              {0, NULL, NULL}};

    if (!gva_metapublish_method_type) {
        gva_metapublish_method_type = g_enum_register_static("GstGVAMetaPublishMethodType", method_types);
    }

    return gva_metapublish_method_type;
}

static void gst_gva_meta_publish_class_init(GstGvaMetaPublishClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(
        GST_ELEMENT_CLASS(klass),
        gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), ELEMENT_LONG_NAME, "Metadata", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_meta_publish_set_property;
    gobject_class->get_property = gst_gva_meta_publish_get_property;

    gobject_class->dispose = gst_gva_meta_publish_dispose;
    gobject_class->finalize = gst_gva_meta_publish_finalize;

    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_set_caps);

    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_stop);

    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_sink_event);

    base_transform_class->before_transform = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_before_transform);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_transform_ip);

    g_object_class_install_property(
        gobject_class, PROP_FILE_PATH,
        g_param_spec_string("filepath", "FilePath",
                            "[method= file] Absolute path to output file for published inferences.", DEFAULT_FILE_PATH,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class, PROP_OUTPUT_FORMAT,
        g_param_spec_string("outputformat", "OutputFormat",
                            "[method= file] Output format of published file. Set to one of: 'stream' (raw inference "
                            "per line) or 'batch' (each file holds array of JSON inferences)",
                            DEFAULT_OUTPUT_FORMAT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    const guint metapublish_prop_len = 128;
    gchar *method_help = g_malloc(metapublish_prop_len * sizeof(gchar));
    g_strlcpy(method_help, "Publishing method. Set to one of: 'file'", metapublish_prop_len);
    if (META_PUBLISH_MQTT) {
        g_strlcat(method_help, ", 'mqtt'", metapublish_prop_len);
    }
    if (META_PUBLISH_KAFKA) {
        g_strlcat(method_help, ", 'kafka'", metapublish_prop_len);
    }
    g_object_class_install_property(gobject_class, PROP_PUBLISH_METHOD,
                                    g_param_spec_enum("method", "Publish method", method_help,
                                                      GST_TYPE_GVA_METAPUBLISH_METHOD, DEFAULT_PUBLISH_METHOD,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    if (META_PUBLISH_MQTT || META_PUBLISH_KAFKA) {
        g_object_class_install_property(gobject_class, PROP_HOST,
                                        g_param_spec_string("host", "Host", "[method= kafka | mqtt] Broker host",
                                                            DEFAULT_HOST, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        g_object_class_install_property(gobject_class, PROP_ADDRESS,
                                        g_param_spec_string("address", "Address",
                                                            "[method= kafka | mqtt] Broker address", DEFAULT_ADDRESS,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        g_object_class_install_property(
            gobject_class, PROP_CLIENTID,
            g_param_spec_string("clientid", "Clientid", "[method= kafka | mqtt] Broker client identifier",
                                DEFAULT_CLIENTID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        g_object_class_install_property(gobject_class, PROP_TIMEOUT,
                                        g_param_spec_string("timeout", "Timeout",
                                                            "[method= kafka | mqtt] Broker timeout", DEFAULT_TIMEOUT,
                                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
        g_object_class_install_property(
            gobject_class, PROP_TOPIC,
            g_param_spec_string("topic", "Topic", "[method= kafka | mqtt] Topic on which to send broker messages",
                                DEFAULT_TOPIC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    }
    g_object_class_install_property(
        gobject_class, PROP_SIGNAL_HANDOFFS,
        g_param_spec_boolean("signal-handoffs", "Signal handoffs", "Send signal before pushing the buffer",
                             DEFAULT_SIGNAL_HANDOFFS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_interpret_signals[SIGNAL_HANDOFF] = g_signal_new(
        "handoff", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(GstGvaMetaPublishClass, handoff), NULL,
        NULL, g_cclosure_marshal_generic, G_TYPE_NONE, 1, GST_TYPE_BUFFER | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void gst_gva_meta_publish_init(GstGvaMetaPublish *gvametapublish) {

    GST_DEBUG_OBJECT(gvametapublish, "gst_gva_meta_publish_init");
    GST_DEBUG_OBJECT(gvametapublish, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvametapublish)));

    gst_gva_meta_publish_reset(gvametapublish);
}

void gst_gva_meta_publish_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(object);

    GST_DEBUG_OBJECT(gvametapublish, "set_property %d", property_id);

    switch (property_id) {
    case PROP_PUBLISH_METHOD:
        gvametapublish->method = g_value_get_enum(value);
        break;
    case PROP_FILE_PATH:
        g_free(gvametapublish->file_path);
        gvametapublish->file_path = g_value_dup_string(value);
        break;
    case PROP_OUTPUT_FORMAT:
        g_free(gvametapublish->output_format);
        gvametapublish->output_format = g_value_dup_string(value);
        break;
    case PROP_HOST:
        g_free(gvametapublish->host);
        gvametapublish->host = g_value_dup_string(value);
        break;
    case PROP_ADDRESS:
        g_free(gvametapublish->address);
        gvametapublish->address = g_value_dup_string(value);
        break;
    case PROP_CLIENTID:
        g_free(gvametapublish->clientid);
        gvametapublish->clientid = g_value_dup_string(value);
        break;
    case PROP_TOPIC:
        g_free(gvametapublish->topic);
        gvametapublish->topic = g_value_dup_string(value);
        break;
    case PROP_TIMEOUT:
        g_free(gvametapublish->timeout);
        gvametapublish->timeout = g_value_dup_string(value);
        break;
    case PROP_SIGNAL_HANDOFFS:
        gvametapublish->signal_handoffs = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_meta_publish_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(object);

    GST_DEBUG_OBJECT(gvametapublish, "get_property");

    switch (property_id) {
    case PROP_PUBLISH_METHOD:
        g_value_set_enum(value, gvametapublish->method);
        break;
    case PROP_FILE_PATH:
        g_value_set_string(value, gvametapublish->file_path);
        break;
    case PROP_OUTPUT_FORMAT:
        g_value_set_string(value, gvametapublish->output_format);
        break;
    case PROP_HOST:
        g_value_set_string(value, gvametapublish->host);
        break;
    case PROP_ADDRESS:
        g_value_set_string(value, gvametapublish->address);
        break;
    case PROP_CLIENTID:
        g_value_set_string(value, gvametapublish->clientid);
        break;
    case PROP_TOPIC:
        g_value_set_string(value, gvametapublish->topic);
        break;
    case PROP_TIMEOUT:
        g_value_set_string(value, gvametapublish->timeout);
        break;
    case PROP_SIGNAL_HANDOFFS:
        g_value_set_boolean(value, gvametapublish->signal_handoffs);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_meta_publish_dispose(GObject *object) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(object);

    GST_DEBUG_OBJECT(gvametapublish, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_meta_publish_parent_class)->dispose(object);
}

void gst_gva_meta_publish_finalize(GObject *object) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(object);

    GST_DEBUG_OBJECT(gvametapublish, "finalize");

    /* clean up object here */
    gst_gva_meta_publish_cleanup(gvametapublish);

    G_OBJECT_CLASS(gst_gva_meta_publish_parent_class)->finalize(object);
}

static void gst_gva_meta_publish_cleanup(GstGvaMetaPublish *gvametapublish) {
    if (gvametapublish == NULL)
        return;

    GST_DEBUG_OBJECT(gvametapublish, "gst_gva_meta_publish_cleanup");

    g_free(gvametapublish->file_path);
    gvametapublish->file_path = NULL;

    g_free(gvametapublish->output_format);
    gvametapublish->output_format = NULL;

    g_free(gvametapublish->host);
    gvametapublish->host = NULL;

    g_free(gvametapublish->address);
    gvametapublish->address = NULL;

    g_free(gvametapublish->clientid);
    gvametapublish->clientid = NULL;

    g_free(gvametapublish->topic);
    gvametapublish->topic = NULL;

    g_free(gvametapublish->timeout);
    gvametapublish->timeout = NULL;
}

static void gst_gva_meta_publish_reset(GstGvaMetaPublish *gvametapublish) {
    GST_DEBUG_OBJECT(gvametapublish, "gst_gva_meta_publish_reset");

    if (gvametapublish == NULL)
        return;

    gst_gva_meta_publish_cleanup(gvametapublish);

    gvametapublish->method = DEFAULT_PUBLISH_METHOD;
    gvametapublish->output_format = g_strdup(DEFAULT_OUTPUT_FORMAT);
    gvametapublish->file_path = g_strdup(DEFAULT_FILE_PATH);
    gvametapublish->host = g_strdup(DEFAULT_HOST);
    gvametapublish->address = g_strdup(DEFAULT_ADDRESS);
    gvametapublish->clientid = g_strdup(DEFAULT_CLIENTID);
    gvametapublish->topic = g_strdup(DEFAULT_TOPIC);
    gvametapublish->timeout = g_strdup(DEFAULT_TIMEOUT);
    gvametapublish->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;
}

static gboolean gst_gva_meta_publish_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {

    UNUSED(incaps);
    UNUSED(outcaps);

    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);
    GST_DEBUG_OBJECT(gvametapublish, "set_caps");

    return TRUE;
}

/* states */
static gboolean gst_gva_meta_publish_start(GstBaseTransform *trans) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    initializeMetaPublishImpl(gvametapublish->method);

    OpenConnection(gvametapublish);
    GST_DEBUG_OBJECT(gvametapublish, "start");

    return TRUE;
}

static gboolean gst_gva_meta_publish_stop(GstBaseTransform *trans) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    GST_DEBUG_OBJECT(gvametapublish, "stop");
    if (gvametapublish == NULL)
        return FALSE;

    CloseConnection(gvametapublish);

    gst_gva_meta_publish_reset(gvametapublish);

    return TRUE;
}

/* sink and src pad event handlers */
static gboolean gst_gva_meta_publish_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    GST_DEBUG_OBJECT(gvametapublish, "sink_event");

    return GST_BASE_TRANSFORM_CLASS(gst_gva_meta_publish_parent_class)->sink_event(trans, event);
}

static void gst_gva_meta_publish_before_transform(GstBaseTransform *trans, GstBuffer *buffer) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    GST_DEBUG_OBJECT(gvametapublish, "before transform");
    GstClockTime timestamp;

    timestamp = gst_segment_to_stream_time(&trans->segment, GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP(buffer));
    GST_LOG_OBJECT(gvametapublish, "Got stream time of %d" GST_TIME_FORMAT, GST_TIME_ARGS(timestamp));
    if (GST_CLOCK_TIME_IS_VALID(timestamp))
        gst_object_sync_values(GST_OBJECT(trans), timestamp);
}

static GstFlowReturn gst_gva_meta_publish_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    if (gvametapublish->signal_handoffs) {

        GST_DEBUG_OBJECT(gvametapublish, "Signal handoffs");
        g_signal_emit(gvametapublish, gst_interpret_signals[SIGNAL_HANDOFF], 0, buf);

    } else {
        WriteMessage(gvametapublish, buf);
    }

    return GST_FLOW_OK;
}
