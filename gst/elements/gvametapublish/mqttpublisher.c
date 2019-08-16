/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "mqttpublisher.h"

#ifdef PAHO_INC
#include <uuid/uuid.h>

MQTTClient mqtt_open_connection(MQTTPublishConfig *gvametapublish) {
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    gint c;

    char *clientid;

    if (gvametapublish->clientid == NULL) {
        uuid_t binuuid;
        uuid_generate_random(binuuid);
        char uuid[37]; // 36 character UUID string plus terminating character
        uuid_unparse(binuuid, uuid);
        clientid = uuid;
    } else {
        clientid = gvametapublish->clientid;
    }

    MQTTClient_create(&client, gvametapublish->bindaddress, clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((c = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        return NULL;
    }
    return client;
}

MetapublishStatusMessage mqtt_close_connection(MQTTClient client) {
    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = MQTT;
    returnMessage.responseCode.mps = MQTT_SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.mps = MQTT_ERROR;
        return returnMessage;
    }

    if (client == NULL) {
        returnMessage.responseCode.mps = MQTT_ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "No client to close\n");
        return returnMessage;
    }
    MQTTClient_disconnect(client, 60);
    MQTTClient_destroy(&client);

    return returnMessage;
}

MetapublishStatusMessage mqtt_write_message(MQTTClient client, MQTTPublishConfig *gvametapublish, GstBuffer *buffer) {
    MQTTClient_message message = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    gulong Timeout;

    if (gvametapublish->timeout == NULL) {
        gvametapublish->timeout = "1000";
    }

    Timeout = *(gvametapublish->timeout);

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = MQTT;
    returnMessage.responseMessage = (gchar *)g_malloc(MAX_RESPONSE_MESSAGE);

    if (client == NULL) {
        returnMessage.responseCode.mps = MQTT_ERROR_NO_CONNECTION;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "No mqtt client connection\n");
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode.mps = MQTT_ERROR_NO_INFERENCE;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "No json metadata found\n");
    } else {
        message.payload = jsonmeta->message;
        message.payloadlen = (gint)strlen(message.payload);
        message.retained = 0;
        MQTTClient_publishMessage(client, gvametapublish->topic, &message, &token);
        MQTTClient_waitForCompletion(client, token, Timeout);
        returnMessage.responseCode.mps = MQTT_SUCCESS;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Message with delivery token %d delivered\n",
                 token);
    }

    return returnMessage;
}
#endif
