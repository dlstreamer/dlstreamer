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

void kafka_open_connection(KafkaPublishConfig *publishConfig, rd_kafka_t *producerHandler,
                           rd_kafka_topic_t *kafka_topic) {
    rd_kafka_conf_t *producerConfig;
    gchar errstr[512];
    producerConfig = rd_kafka_conf_new();

    if (rd_kafka_conf_set(producerConfig, "bootstrap.servers", publishConfig->address, errstr, sizeof(errstr)) !=
        RD_KAFKA_CONF_OK) {
        rd_kafka_conf_destroy(producerConfig);
        return;
    }

    rd_kafka_conf_set_dr_msg_cb(producerConfig, message_callback);

    producerHandler = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig, errstr, sizeof(errstr));
    if (!producerHandler) {
        return;
    }

    kafka_topic = rd_kafka_topic_new(producerHandler, publishConfig->topic, NULL);
    if (!kafka_topic) {
        rd_kafka_destroy(producerHandler);
        return;
    }
}

void kafka_close_connection(rd_kafka_t *producerHandler, rd_kafka_topic_t *kafka_topic) {
    while (rd_kafka_outq_len(producerHandler) > 0)
        rd_kafka_poll(producerHandler, 0);
    rd_kafka_topic_destroy(kafka_topic);
    rd_kafka_destroy(producerHandler);
}

/*
 * Try to publish a message to a kafka queue and returns a message back. It is
 * possible for the returned KafkaStatusMessage.responseMessage to be NULL so
 * should be checked by the caller of this function.
 */
MetapublishStatusMessage kafka_write_message(rd_kafka_t *producerHandler, rd_kafka_topic_t *kafka_topic,
                                             GstBuffer *buffer) {
    gint msg;
    void *payload = NULL;

    const int MAX_KAFKA_RESPONSE_SIZE = 1024;
    MetapublishStatusMessage returnMessage;
    returnMessage.responseCode = 0;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_KAFKA_RESPONSE_SIZE);
    if (returnMessage.responseMessage == NULL) {
        return returnMessage;
    }

    if (producerHandler == NULL) {
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode = -1;
        snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "no json metadata found\n");
        return returnMessage;
    } else {
        payload = jsonmeta->message;

        msg = rd_kafka_produce(kafka_topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, payload, strlen(payload), NULL,
                               0, NULL);
        if (msg == -1) {
            returnMessage.responseCode = -1;
            snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Failed to produce to topic\n");
            return returnMessage;
        }
    }

    while (rd_kafka_outq_len(producerHandler) > 0)
        rd_kafka_poll(producerHandler, 0);

    snprintf(returnMessage.responseMessage, MAX_KAFKA_RESPONSE_SIZE, "Kafka message sent successfully\n");
    return returnMessage;
}

#endif
