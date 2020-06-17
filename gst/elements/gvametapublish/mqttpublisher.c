/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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

    MQTTClient_create(&client, gvametapublish->address, clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL);
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

    if (client == NULL) {
        returnMessage.responseCode.mps = MQTT_ERROR;
        prepare_response_message(&returnMessage, "No client to close\n");
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

    if (client == NULL) {
        returnMessage.responseCode.mps = MQTT_ERROR_NO_CONNECTION;
        prepare_response_message(&returnMessage, "No mqtt client connection\n");
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode.mps = MQTT_ERROR_NO_INFERENCE;
        prepare_response_message(&returnMessage, "No json metadata found\n");
    } else {
        message.payload = jsonmeta->message;
        message.payloadlen = (gint)strlen(message.payload);
        message.retained = 0;
        MQTTClient_publishMessage(client, gvametapublish->topic, &message, &token);
        MQTTClient_waitForCompletion(client, token, Timeout);
        returnMessage.responseCode.mps = MQTT_SUCCESS;
        prepare_response_message(&returnMessage, "Message with delivery token delivered\n");
    }

    return returnMessage;
}
#endif
