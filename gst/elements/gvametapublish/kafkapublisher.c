/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "kafkapublisher.h"
#define UNUSED(x) (void)(x)

#ifdef KAFKA_INC
static void message_callback(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {

    UNUSED(opaque);
    UNUSED(rk);

    if (rkmessage->err)
        GST_DEBUG("Message delivery failed");
    else
        GST_INFO("Message delivered");
}

/*
 * Try to publish a message to a kafka queue and returns a message back. It is
 * possible for the returned KafkaStatusMessage.responseMessage to be NULL so
 * should be checked by the caller of this function.
 */
KafkaStatusMessage kafka_publish(KafkaPublishConfig *gvametapublish, GstBuffer *buffer) {
    rd_kafka_t *producerHandler;
    rd_kafka_conf_t *producerConfig;
    rd_kafka_topic_t *rkt;

    gchar errstr[512];
    gint msg;
    void *payload = NULL;

    const int MAX_KAFKA_RESPONSE_SIZE = 1024;
    KafkaStatusMessage returnMessage;
    returnMessage.responseCode = 0;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_KAFKA_RESPONSE_SIZE);
    if (returnMessage.responseMessage == NULL) {
        return returnMessage;
    }

    producerConfig = rd_kafka_conf_new();

    if (rd_kafka_conf_set(producerConfig, "bootstrap.servers", gvametapublish->address, errstr, sizeof(errstr)) !=
        RD_KAFKA_CONF_OK) {
        rd_kafka_conf_destroy(producerConfig);
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "%s\n", errstr);
        return returnMessage;
    }

    rd_kafka_conf_set_dr_msg_cb(producerConfig, message_callback);

    producerHandler = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig, errstr, sizeof(errstr));
    if (!producerHandler) {
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Failed to create new producer\n");
        return returnMessage;
    }

    rkt = rd_kafka_topic_new(producerHandler, gvametapublish->topic, NULL);
    if (!rkt) {
        rd_kafka_destroy(producerHandler);
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Failed to create topic object\n");
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "no json metadata found\n");
        return returnMessage;
    } else {
        payload = jsonmeta->message;

        msg =
            rd_kafka_produce(rkt, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, payload, strlen(payload), NULL, 0, NULL);
        if (msg == -1) {
            returnMessage.responseCode = -1;
            snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Failed to produce to topic\n");
            return returnMessage;
        }
    }

    while (rd_kafka_outq_len(producerHandler) > 0)
        rd_kafka_poll(producerHandler, 0);

    rd_kafka_topic_destroy(rkt);

    rd_kafka_destroy(producerHandler);

    snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Kafka message sent successfully\n");
    return returnMessage;
}
#endif
