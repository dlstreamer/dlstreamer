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

MetapublishStatusMessage kafka_open_connection(KafkaPublishConfig *publishConfig, rd_kafka_t *producerHandler,
                                               rd_kafka_topic_t *kafka_topic) {
    rd_kafka_conf_t *producerConfig;
    gchar errstr[512];
    producerConfig = rd_kafka_conf_new();

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        return returnMessage;
    }

    if (rd_kafka_conf_set(producerConfig, "bootstrap.servers", publishConfig->address, errstr, sizeof(errstr)) !=
        RD_KAFKA_CONF_OK) {
        rd_kafka_conf_destroy(producerConfig);
    }

    rd_kafka_conf_set_dr_msg_cb(producerConfig, message_callback);

    producerHandler = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig, errstr, sizeof(errstr));
    if (!producerHandler) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to create Producer Handler\n");
        return returnMessage;
    }

    kafka_topic = rd_kafka_topic_new(producerHandler, publishConfig->topic, NULL);
    if (!kafka_topic) {
        rd_kafka_destroy(producerHandler);
        returnMessage.responseCode.kps = KAFKA_ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to create new topic\n");
        return returnMessage;
    }

    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Kafka connection opened successfully\n");
    return returnMessage;
}

MetapublishStatusMessage kafka_close_connection(rd_kafka_t *producerHandler, rd_kafka_topic_t *kafka_topic) {
    while (rd_kafka_outq_len(producerHandler) > 0)
        rd_kafka_poll(producerHandler, 0);
    rd_kafka_topic_destroy(kafka_topic);
    rd_kafka_destroy(producerHandler);

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Kafka connection closed successfully\n");

    return returnMessage;
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

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        return returnMessage;
    }

    if (producerHandler == NULL) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode.kps = KAFKA_ERROR_NO_INFERENCE;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "no json metadata found\n");
        return returnMessage;
    } else {
        payload = jsonmeta->message;

        msg = rd_kafka_produce(kafka_topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, payload, strlen(payload), NULL,
                               0, NULL);
        if (msg == -1) {
            returnMessage.responseCode.kps = KAFKA_ERROR_NO_TOPIC_PRODUCED;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to produce to topic\n");
            return returnMessage;
        }
    }

    while (rd_kafka_outq_len(producerHandler) > 0)
        rd_kafka_poll(producerHandler, 0);

    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Kafka message sent successfully\n");
    return returnMessage;
}

#endif
