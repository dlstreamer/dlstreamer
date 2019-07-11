/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "mqttpublisher.h"

#ifdef PAHO_INC
MQTTClient mqtt_open_connection(MQTTPublishConfig *gvametapublish) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    gint c;

    MQTTClient_create(&client, gvametapublish->bindaddress, gvametapublish->clientid, MQTTCLIENT_PERSISTENCE_NONE,
                      NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((c = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        return NULL;
    }
    return client;
}

void mqtt_close_connection(MQTTClient client) {
    MQTTClient_disconnect(client, 60);
    MQTTClient_destroy(&client);
}

MetapublishStatusMessage mqtt_write_message(MQTTClient client, MQTTPublishConfig *gvametapublish, GstBuffer *buffer) {
    MQTTClient_message message = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    gulong Timeout;
    Timeout = *(gvametapublish->timeout);

    MetapublishStatusMessage returnMessage;
    returnMessage.responseMessage = (gchar *)g_malloc(1024);

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode = 0;
        snprintf(returnMessage.responseMessage, 1024, "no json metadata found\n");
    } else {
        message.payload = jsonmeta->message;
        message.payloadlen = (gint)strlen(message.payload);
        message.retained = 0;
        MQTTClient_publishMessage(client, gvametapublish->topic, &message, &token);
        MQTTClient_waitForCompletion(client, token, Timeout);
        returnMessage.responseCode = 0;
        snprintf(returnMessage.responseMessage, 1024, "Message with delivery token %d delivered\n", token);
    }

    return returnMessage;
}
#endif
