/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "c_metapublish_mqtt.h"
#ifdef PAHO_INC
#include <uuid/uuid.h>

GST_DEBUG_CATEGORY_STATIC(gst_gva_meta_publish_debug_category);
#define GST_CAT_DEFAULT gst_gva_meta_publish_debug_category

struct _MetapublishMQTT {
    GObject parent_instance;
    MQTTAsync *client;
    GstGvaMetaPublish *gvametapublish;
    guint connection_attempt;
    guint sleep_time;
};

static void metapublish_mqtt_method_interface_init(MetapublishMethodInterface *iface);

G_DEFINE_TYPE_WITH_CODE(MetapublishMQTT, metapublish_mqtt, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(METAPUBLISH_TYPE_METHOD, metapublish_mqtt_method_interface_init))

static gboolean metapublish_mqtt_method_start(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    GST_DEBUG_CATEGORY_INIT(gst_gva_meta_publish_debug_category, "gvametapublish", 0,
                            "debug category for gvametapublish element");
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(self);
    mp_mqtt->client = g_malloc(sizeof(*(mp_mqtt->client)));
    if (!mp_mqtt->client) {
        GST_ERROR_OBJECT(gvametapublish, "Could not allocate memory for MQTT handle");
        return FALSE;
    }
    mp_mqtt->gvametapublish = gvametapublish;
    mp_mqtt->connection_attempt = 1;
    mp_mqtt->sleep_time = 1;
    if (!gvametapublish->mqtt_client_id) {
        uuid_t binuuid;
        uuid_generate_random(binuuid);
        char *uuid = g_malloc(37 * sizeof(char)); // 36 character UUID string plus terminating character
        uuid_unparse(binuuid, uuid);
        gvametapublish->mqtt_client_id = uuid;
    }
    MQTTAsync_create(mp_mqtt->client, gvametapublish->address, gvametapublish->mqtt_client_id,
                     MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTAsync_setCallbacks(*(mp_mqtt->client), mp_mqtt, connection_lost, message_arrived, delivery_complete);
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = on_connect_success;
    conn_opts.onFailure = on_connect_failure;
    conn_opts.context = mp_mqtt;
    gint c;
    if ((c = MQTTAsync_connect(*(mp_mqtt->client), &conn_opts)) != MQTTASYNC_SUCCESS) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to start connection attempt to MQTT. Error code %d.", c);
        return FALSE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "Connect request sent to MQTT.");
    return TRUE;
}

static gboolean metapublish_mqtt_method_publish(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish,
                                                gchar *json_message) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(self);
    if (!mp_mqtt->client) {
        GST_ERROR_OBJECT(gvametapublish, "MQTT client is null. Cannot publish message.");
        return FALSE;
    }
    if (!json_message) {
        GST_DEBUG_OBJECT(gvametapublish, "No JSON message.");
        return TRUE;
    }
    MQTTAsync client = (MQTTAsync) * (mp_mqtt->client);
    MQTTAsync_message message = MQTTAsync_message_initializer;
    message.payload = json_message;
    message.payloadlen = (gint)strlen(message.payload);
    message.retained = FALSE;
    gint c;
    // TODO Validate message is JSON
    MQTTAsync_responseOptions ro = MQTTAsync_responseOptions_initializer;
    ro.onSuccess = on_send_success;
    ro.onFailure = on_send_failure;
    ro.context = mp_mqtt;

    if ((c = MQTTAsync_sendMessage(client, gvametapublish->topic, &message, &ro)) != MQTTASYNC_SUCCESS) {
        GST_ERROR_OBJECT(gvametapublish, "Message was not accepted for publication. Error code %d.", c);
        return TRUE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "MQTT message sent.");
    return TRUE;
}

static gboolean metapublish_mqtt_method_stop(MetapublishMethod *self, GstGvaMetaPublish *gvametapublish) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(self);
    if (!mp_mqtt->client) {
        GST_ERROR_OBJECT(gvametapublish, "No MQTT client was initialized. Nothing to disconnect.");
        return TRUE;
    }
    MQTTAsync client = (MQTTAsync) * (mp_mqtt->client);
    if (!MQTTAsync_isConnected(client)) {
        GST_DEBUG_OBJECT(gvametapublish, "MQTT client is not connected. Nothing to disconnect");
        return TRUE;
    }
    MQTTAsync_disconnectOptions disconnect_options = MQTTAsync_disconnectOptions_initializer;
    disconnect_options.onSuccess = on_disconnect_success;
    disconnect_options.onFailure = on_disconnect_failure;
    disconnect_options.context = mp_mqtt;

    gint c = MQTTAsync_disconnect(client, &disconnect_options);
    if (c != MQTTASYNC_SUCCESS) {
        GST_ERROR_OBJECT(gvametapublish, "Disconnection from MQTT failed with error code %d.", c);
        return FALSE;
    }
    GST_DEBUG_OBJECT(gvametapublish, "Disconnect request sent to MQTT.");
    return TRUE;
}

static void metapublish_mqtt_finalize(GObject *gobject) {
    MetapublishMQTT *mp_mqtt = metapublish_mqtt_get_instance_private(METAPUBLISH_MQTT(gobject));

    MQTTAsync_destroy(mp_mqtt->client);
    g_free(mp_mqtt->client);
    mp_mqtt->client = NULL;
    GST_DEBUG("Successfully freed MQTT client.");

    G_OBJECT_CLASS(metapublish_mqtt_parent_class)->finalize(gobject);
}

static void metapublish_mqtt_method_interface_init(MetapublishMethodInterface *iface) {
    iface->start = metapublish_mqtt_method_start;
    iface->publish = metapublish_mqtt_method_publish;
    iface->stop = metapublish_mqtt_method_stop;
}

static void metapublish_mqtt_init(MetapublishMQTT *self) {
    (void)self;
}

static void metapublish_mqtt_class_init(MetapublishMQTTClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = metapublish_mqtt_finalize;
}

// MQTT CALLBACKS
void connection_lost(void *context, char *cause) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    MQTTAsync client = (MQTTAsync) * (mp_mqtt->client);
    if (mp_mqtt->connection_attempt == mp_mqtt->gvametapublish->max_connect_attempts) {
        GST_ELEMENT_ERROR(mp_mqtt->gvametapublish, RESOURCE, NOT_FOUND,
                          ("Failed to connect to MQTT after maximum configured attempts."), (NULL));
        return;
    }
    mp_mqtt->connection_attempt++;
    mp_mqtt->sleep_time *= 2;
    if (mp_mqtt->sleep_time > mp_mqtt->gvametapublish->max_reconnect_interval) {
        mp_mqtt->sleep_time = mp_mqtt->gvametapublish->max_reconnect_interval;
    }
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    GST_WARNING_OBJECT(mp_mqtt->gvametapublish, "Connection to MQTT lost. Cause: %s. Attempting to reconnect", cause);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = on_connect_success;
    conn_opts.onFailure = on_connect_failure;
    conn_opts.context = mp_mqtt;
    gint c;
    g_usleep(mp_mqtt->sleep_time * G_USEC_PER_SEC);
    if ((c = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        GST_ERROR_OBJECT(mp_mqtt->gvametapublish, "Failed to start connection attempt to MQTT. Error code %d.", c);
    }
}

int message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    (void)context;
    (void)topicName;
    (void)topicLen;
    (void)message;
    return TRUE;
}

void delivery_complete(void *context, MQTTAsync_token token) {
    (void)context;
    (void)token;
}

void on_connect_success(void *context, MQTTAsync_successData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_connect_success callback received null context");
        GST_DEBUG("Successfully connected to MQTT");
        return;
    }
    GST_DEBUG_OBJECT(mp_mqtt->gvametapublish, "Successfully connected to MQTT");
    (void)response;
}

void on_connect_failure(void *context, MQTTAsync_failureData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_connect_failure callback received null context");
        return;
    }
    GST_WARNING_OBJECT(mp_mqtt->gvametapublish, "Connection attempt to MQTT failed.");
    if (mp_mqtt->connection_attempt == mp_mqtt->gvametapublish->max_connect_attempts) {
        GST_ELEMENT_ERROR(mp_mqtt->gvametapublish, RESOURCE, NOT_FOUND,
                          ("Failed to connect to MQTT after maximum configured attempts."), (NULL));
        return;
    }
    mp_mqtt->connection_attempt++;
    mp_mqtt->sleep_time *= 2;
    if (mp_mqtt->sleep_time > mp_mqtt->gvametapublish->max_reconnect_interval) {
        mp_mqtt->sleep_time = mp_mqtt->gvametapublish->max_reconnect_interval;
    }
    MQTTAsync client = (MQTTAsync) * (mp_mqtt->client);
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
    gint c;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.onSuccess = on_connect_success;
    conn_opts.onFailure = on_connect_failure;
    conn_opts.context = mp_mqtt;
    g_usleep(mp_mqtt->sleep_time * G_USEC_PER_SEC);
    GST_DEBUG_OBJECT(mp_mqtt->gvametapublish, "Attempt %d to connect to MQTT again.", mp_mqtt->connection_attempt);
    if ((c = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS) {
        GST_ERROR("Failed to start connection attempt to MQTT. Error code %d.", c);
    }
    (void)response;
}

void on_send_success(void *context, MQTTAsync_successData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_send_success callback received null context");
        GST_DEBUG("Message successfully published to MQTT");
        return;
    }
    GST_DEBUG_OBJECT(mp_mqtt->gvametapublish, "Message successfully published to MQTT");
    (void)response;
}
void on_send_failure(void *context, MQTTAsync_failureData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_send_failure callback received null context");
        GST_ERROR("Message failed to publish to MQTT");
        return;
    }
    GST_ERROR_OBJECT(mp_mqtt->gvametapublish, "Message failed to publish to MQTT");
    (void)response;
}

void on_disconnect_success(void *context, MQTTAsync_successData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_disconnect_success callback received null context");
        GST_DEBUG("Successfully disconnected from MQTT.");
        return;
    }
    GST_DEBUG_OBJECT(mp_mqtt->gvametapublish, "Successfully disconnected from MQTT.");
    (void)response;
}
void on_disconnect_failure(void *context, MQTTAsync_failureData *response) {
    MetapublishMQTT *mp_mqtt = METAPUBLISH_MQTT(context);
    if (!mp_mqtt) {
        GST_ERROR("Metapublish_MQTT on_disconnect_failure callback received null context");
        GST_ERROR("Failed to disconnect from MQTT.");
        return;
    }
    GST_ERROR_OBJECT(mp_mqtt->gvametapublish, "Failed to disconnect from MQTT.");
    (void)response;
}

#endif /* PAHO_INC */