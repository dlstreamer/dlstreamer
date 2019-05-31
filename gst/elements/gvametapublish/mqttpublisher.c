/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "mqttpublisher.h"

#ifdef PAHO_INC
MQTTStatusMessage mqtt_publish(MQTTPublishConfig *gvametapublish, GstBuffer *buffer) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message message = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    gint c;
    gulong Timeout;
    Timeout = *(gvametapublish->timeout);

    MQTTClient_create(&client, gvametapublish->bindaddress, gvametapublish->clientid, MQTTCLIENT_PERSISTENCE_NONE,
                      NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTStatusMessage returnMessage;
    returnMessage.responseMessage = (gchar *)g_malloc(1024);

    if ((c = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, 1024, "failed to connect\n");
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode = 0;
        snprintf(returnMessage.responseMessage, 1024, "no json metadata found\n");
    } else {
        message.payload = jsonmeta->message;
        message.payloadlen = (gint)strlen(message.payload);
        message.retained = 0;
        MQTTClient_publishMessage(client, gvametapublish->topic, &message, &token);
        c = MQTTClient_waitForCompletion(client, token, Timeout);
        returnMessage.responseCode = 0;
        snprintf(returnMessage.responseMessage, 1024, "Message with delivery token %d delivered\n", token);
    }
    MQTTClient_disconnect(client, 60);
    MQTTClient_destroy(&client);
    return returnMessage;
}
#endif
