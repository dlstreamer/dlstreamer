/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvametapublish.h"
#include "c_metapublish_file.h"
#ifdef PAHO_INC
#include "c_metapublish_mqtt.h"
#endif /* PAHO_INC */
#ifdef KAFKA_INC
#include "c_metapublish_kafka.h"
#endif /* KAFKA_INC */
#include "gva_caps.h"
#include "gva_json_meta.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_publish_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_publish_debug_category

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Generic metadata publisher"
#define ELEMENT_DESCRIPTION "Publishes the JSON metadata to MQTT or Kafka message brokers or files."

static GstStaticPadTemplate sink_factory =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

static GstStaticPadTemplate src_factory =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

/* prototypes */

static void gst_gva_meta_publish_set_property(GObject *object, guint property_id, const GValue *value,
                                              GParamSpec *pspec);
static void gst_gva_meta_publish_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_meta_publish_dispose(GObject *object);
static void gst_gva_meta_publish_finalize(GObject *object);

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
#define DEFAULT_FILE_PATH STDOUT
#define DEFAULT_FILE_FORMAT GST_GVA_METAPUBLISH_JSON

// Broker specific constants
#define DEFAULT_ADDRESS NULL
#define DEFAULT_MQTTCLIENTID NULL
#define DEFAULT_TOPIC NULL
#define DEFAULT_SIGNAL_HANDOFFS FALSE
#define DEFAULT_TIMEOUT NULL
#define DEFAULT_MAX_CONNECT_ATTEMPTS 1
#define DEFAULT_MAX_RECONNECT_INTERVAL 30

enum {
    PROP_0,
    PROP_PUBLISH_METHOD,
    PROP_FILE_PATH,
    PROP_FILE_FORMAT,
    PROP_ADDRESS,
    PROP_MQTTCLIENTID,
    PROP_TOPIC,
    PROP_TIMEOUT,
    PROP_MAX_CONNECT_ATTEMPTS,
    PROP_MAX_RECONNECT_INTERVAL,
    PROP_SIGNAL_HANDOFFS,
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaMetaPublish, gst_gva_meta_publish, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_meta_publish_debug_category, "gvametapublish", 0,
                                                "debug category for gvametapublish element"));

#define GST_TYPE_GVA_METAPUBLISH_METHOD (gst_gva_metapublish_method_get_type())
static GType gst_gva_metapublish_method_get_type(void) {
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
        gva_metapublish_method_type = g_enum_register_static("GstGVAMetaPublishMethod", method_types);
    }

    return gva_metapublish_method_type;
}

#define GST_TYPE_GVA_METAPUBLISH_FILE_FORMAT (gst_gva_metapublish_file_format_get_type())
static GType gst_gva_metapublish_file_format_get_type(void) {
    static GType gva_metapublish_file_format_type = 0;
    static const GEnumValue file_format_types[] = {
        {GST_GVA_METAPUBLISH_JSON,
         "the whole file is valid JSON array where each element is inference results per frame", "json"},
        {GST_GVA_METAPUBLISH_JSON_LINES, "each line is valid JSON with inference results per frame", "json-lines"},
        {0, NULL, NULL}};

    if (!gva_metapublish_file_format_type) {
        gva_metapublish_file_format_type = g_enum_register_static("GstGVAMetaPublishFileFormat", file_format_types);
    }

    return gva_metapublish_file_format_type;
}

static void gst_gva_meta_publish_class_init(GstGvaMetaPublishClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass), gst_static_pad_template_get(&src_factory));
    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass), gst_static_pad_template_get(&sink_factory));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), ELEMENT_LONG_NAME, "Metadata", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_meta_publish_set_property;
    gobject_class->get_property = gst_gva_meta_publish_get_property;

    gobject_class->dispose = gst_gva_meta_publish_dispose;
    gobject_class->finalize = gst_gva_meta_publish_finalize;

    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_stop);

    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_sink_event);

    base_transform_class->before_transform = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_before_transform);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_meta_publish_transform_ip);

    g_object_class_install_property(
        gobject_class, PROP_FILE_PATH,
        g_param_spec_string("file-path", "FilePath",
                            "[method= file] Absolute path to output file for publishing inferences.", DEFAULT_FILE_PATH,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_FILE_FORMAT,
                                    g_param_spec_enum("file-format", "File Format",
                                                      "[method= file] Structure of JSON objects in the file",
                                                      GST_TYPE_GVA_METAPUBLISH_FILE_FORMAT, DEFAULT_FILE_FORMAT,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    const guint metapublish_prop_len = 128;
    gchar *method_help = g_malloc(metapublish_prop_len * sizeof(gchar));
    g_strlcpy(method_help, "Publishing method. Set to one of: 'file'", metapublish_prop_len);
#ifdef PAHO_INC
    g_strlcat(method_help, ", 'mqtt'", metapublish_prop_len);
#endif /* PAHO_INC */
#ifdef KAFKA_INC
    g_strlcat(method_help, ", 'kafka'", metapublish_prop_len);
#endif /* KAFKA_INC */

    g_object_class_install_property(gobject_class, PROP_PUBLISH_METHOD,
                                    g_param_spec_enum("method", "Publish method", method_help,
                                                      GST_TYPE_GVA_METAPUBLISH_METHOD, DEFAULT_PUBLISH_METHOD,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#if defined(PAHO_INC) || defined(KAFKA_INC)
    g_object_class_install_property(gobject_class, PROP_ADDRESS,
                                    g_param_spec_string("address", "Address", "[method= kafka | mqtt] Broker address",
                                                        DEFAULT_ADDRESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_MQTTCLIENTID,
                                    g_param_spec_string("mqtt-client-id", "MQTT Client ID",
                                                        "[method= mqtt] Unique identifier for the MQTT "
                                                        "client. If not provided, one will be generated for you.",
                                                        DEFAULT_MQTTCLIENTID,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class, PROP_TIMEOUT,
        g_param_spec_string("timeout", "Timeout", "[method= kafka | mqtt] Broker timeout", DEFAULT_TIMEOUT,
                            G_PARAM_DEPRECATED | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_TOPIC,
                                    g_param_spec_string("topic", "Topic",
                                                        "[method= kafka | mqtt] Topic on which to send broker messages",
                                                        DEFAULT_TOPIC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_MAX_CONNECT_ATTEMPTS,
                                    g_param_spec_uint("max-connect-attempts", "Max Connect Attempts",
                                                      "[method= kafka | mqtt] Maximum number of failed connection "
                                                      "attempts before it is considered fatal.",
                                                      1, 10, DEFAULT_MAX_CONNECT_ATTEMPTS,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class, PROP_MAX_RECONNECT_INTERVAL,
        g_param_spec_uint("max-reconnect-interval", "Max Reconnect Interval",
                          "[method= kafka | mqtt] Maximum time in seconds between reconnection attempts. Initial "
                          "interval is 1 second and will be doubled on each failure up to this maximum interval.",
                          1, 300, DEFAULT_MAX_RECONNECT_INTERVAL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif
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
    case PROP_FILE_FORMAT:
        gvametapublish->file_format = g_value_get_enum(value);
        break;
    case PROP_ADDRESS:
        g_free(gvametapublish->address);
        gvametapublish->address = g_value_dup_string(value);
        break;
    case PROP_MQTTCLIENTID:
        g_free(gvametapublish->mqtt_client_id);
        gvametapublish->mqtt_client_id = g_value_dup_string(value);
        break;
    case PROP_TOPIC:
        g_free(gvametapublish->topic);
        gvametapublish->topic = g_value_dup_string(value);
        break;
    case PROP_TIMEOUT:
        GST_WARNING_OBJECT(gvametapublish, "The property \'timeout\' for gvametapublish is deprecated and should not "
                                           "be used anymore. It will be removed from a future version.");
        g_free(gvametapublish->timeout);
        gvametapublish->timeout = g_value_dup_string(value);
        break;
    case PROP_MAX_CONNECT_ATTEMPTS:
        gvametapublish->max_connect_attempts = g_value_get_uint(value);
        break;
    case PROP_MAX_RECONNECT_INTERVAL:
        gvametapublish->max_reconnect_interval = g_value_get_uint(value);
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
    case PROP_FILE_FORMAT:
        g_value_set_enum(value, gvametapublish->file_format);
        break;
    case PROP_ADDRESS:
        g_value_set_string(value, gvametapublish->address);
        break;
    case PROP_MQTTCLIENTID:
        g_value_set_string(value, gvametapublish->mqtt_client_id);
        break;
    case PROP_TOPIC:
        g_value_set_string(value, gvametapublish->topic);
        break;
    case PROP_TIMEOUT:
        g_value_set_string(value, gvametapublish->timeout);
        break;
    case PROP_MAX_CONNECT_ATTEMPTS:
        g_value_set_uint(value, gvametapublish->max_connect_attempts);
        break;
    case PROP_MAX_RECONNECT_INTERVAL:
        g_value_set_uint(value, gvametapublish->max_reconnect_interval);
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
    if (!gvametapublish)
        return;

    GST_DEBUG_OBJECT(gvametapublish, "gst_gva_meta_publish_cleanup");

    g_free(gvametapublish->file_path);
    gvametapublish->file_path = NULL;

    g_free(gvametapublish->address);
    gvametapublish->address = NULL;

    g_free(gvametapublish->mqtt_client_id);
    gvametapublish->mqtt_client_id = NULL;

    g_free(gvametapublish->topic);
    gvametapublish->topic = NULL;

    g_free(gvametapublish->timeout);
    gvametapublish->timeout = NULL;

    if (gvametapublish->method_class)
        g_clear_object(&(gvametapublish->method_class));
}

static void gst_gva_meta_publish_reset(GstGvaMetaPublish *gvametapublish) {
    GST_DEBUG_OBJECT(gvametapublish, "gst_gva_meta_publish_reset");

    if (!gvametapublish)
        return;

    gst_gva_meta_publish_cleanup(gvametapublish);

    gvametapublish->method = DEFAULT_PUBLISH_METHOD;
    gvametapublish->file_format = DEFAULT_FILE_FORMAT;
    gvametapublish->file_path = g_strdup(DEFAULT_FILE_PATH);
    gvametapublish->address = g_strdup(DEFAULT_ADDRESS);
    gvametapublish->mqtt_client_id = g_strdup(DEFAULT_MQTTCLIENTID);
    gvametapublish->topic = g_strdup(DEFAULT_TOPIC);
    gvametapublish->timeout = g_strdup(DEFAULT_TIMEOUT);
    gvametapublish->max_connect_attempts = DEFAULT_MAX_CONNECT_ATTEMPTS;
    gvametapublish->max_reconnect_interval = DEFAULT_MAX_RECONNECT_INTERVAL;
    gvametapublish->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;
}

/* states */
static gboolean gst_gva_meta_publish_start(GstBaseTransform *trans) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);
    GST_DEBUG_OBJECT(gvametapublish, "start");

    if (!gvametapublish) {
        return FALSE;
    }

    switch (gvametapublish->method) {
    case GST_GVA_METAPUBLISH_FILE:
        gvametapublish->method_class = g_object_new(METAPUBLISH_TYPE_FILE, NULL);
        break;
#ifdef PAHO_INC
    case GST_GVA_METAPUBLISH_MQTT:
        gvametapublish->method_class = g_object_new(METAPUBLISH_TYPE_MQTT, NULL);
        break;
#endif /* PAHO_INC */
#ifdef KAFKA_INC
    case GST_GVA_METAPUBLISH_KAFKA:
        gvametapublish->method_class = g_object_new(METAPUBLISH_TYPE_KAFKA, NULL);
        break;
#endif /* KAFKA_INC */
    default:
        GST_ERROR_OBJECT(gvametapublish, "\'method\' property set to invalid value");
        break;
    }
    if (!metapublish_method_start(gvametapublish->method_class, gvametapublish)) {
        GST_ELEMENT_ERROR(gvametapublish, RESOURCE, NOT_FOUND, ("Failed to start"),
                          ("Failed to open a connection for method %s",
                           g_enum_to_string(GST_TYPE_GVA_METAPUBLISH_METHOD, gvametapublish->method)));
        return FALSE;
    }
    return TRUE;
}

static gboolean gst_gva_meta_publish_stop(GstBaseTransform *trans) {
    GstGvaMetaPublish *gvametapublish = GST_GVA_META_PUBLISH(trans);

    GST_DEBUG_OBJECT(gvametapublish, "stop");
    if (!gvametapublish)
        return FALSE;
    if (!metapublish_method_stop(gvametapublish->method_class, gvametapublish)) {
        GST_ELEMENT_ERROR(gvametapublish, RESOURCE, NOT_FOUND, ("Failed to stop"),
                          ("Failed to close connection for method %s",
                           g_enum_to_string(GST_TYPE_GVA_METAPUBLISH_METHOD, gvametapublish->method)));
        return FALSE;
    }

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
    GST_DEBUG_OBJECT(gvametapublish, "transform ip");
    GstGVAJSONMeta *json_meta = GST_GVA_JSON_META_GET(buf);
    if (!gvametapublish) {
        GST_ELEMENT_ERROR(gvametapublish, CORE, FAILED, ("gvametapublish element was not initialized properly"),
                          (NULL));
        return GST_FLOW_ERROR;
    }
    if (gvametapublish->signal_handoffs) {
        GST_DEBUG_OBJECT(gvametapublish, "Signal handoffs");
        g_signal_emit(gvametapublish, gst_interpret_signals[SIGNAL_HANDOFF], 0, buf);
    }
    if (!json_meta) {
        GST_DEBUG_OBJECT(gvametapublish, "No JSON metadata");
        return GST_FLOW_OK;
    }
    if (!metapublish_method_publish(gvametapublish->method_class, gvametapublish, json_meta->message)) {
        GST_ELEMENT_ERROR(gvametapublish, RESOURCE, NOT_FOUND, ("Failed to publish message"),
                          ("Failed to publish message for method %s.",
                           g_enum_to_string(GST_TYPE_GVA_METAPUBLISH_METHOD, gvametapublish->method)));
        return GST_FLOW_ERROR;
    }
    return GST_FLOW_OK;
}
