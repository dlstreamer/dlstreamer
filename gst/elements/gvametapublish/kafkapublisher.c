/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "kafkapublisher.h"
#define UNUSED(x) (void)(x)

#ifdef KAFKA_INC
MetapublishStatusMessage kafka_open_connection(KafkaPublishConfig *publishConfig, rd_kafka_t **producerHandler,
                                               rd_kafka_topic_t **kafka_topic) {
    rd_kafka_conf_t *producerConfig;
    gchar errstr[512];
    producerConfig = rd_kafka_conf_new();

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;

    if (rd_kafka_conf_set(producerConfig, "bootstrap.servers", publishConfig->address, errstr, sizeof(errstr)) !=
        RD_KAFKA_CONF_OK) {
        rd_kafka_conf_destroy(producerConfig);
        returnMessage.responseCode.kps = KAFKA_ERROR;
        prepare_response_message(&returnMessage, "Failed to establish connection to kafka server\n");
        return returnMessage;
    }

    *producerHandler = rd_kafka_new(RD_KAFKA_PRODUCER, producerConfig, errstr, sizeof(errstr));
    if (!*producerHandler) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        prepare_response_message(&returnMessage, "Failed to create Producer Handler\n");
        return returnMessage;
    }

    *kafka_topic = rd_kafka_topic_new(*producerHandler, publishConfig->topic, NULL);
    if (!*kafka_topic) {
        rd_kafka_destroy(*producerHandler);
        returnMessage.responseCode.kps = KAFKA_ERROR;
        prepare_response_message(&returnMessage, "Failed to create new topic\n");
        return returnMessage;
    }

    prepare_response_message(&returnMessage, "Kafka connection opened successfully\n");
    return returnMessage;
}

MetapublishStatusMessage kafka_close_connection(rd_kafka_t **producerHandler, rd_kafka_topic_t **kafka_topic) {
    while (rd_kafka_outq_len(*producerHandler) > 0)
        rd_kafka_poll(*producerHandler, 0);
    rd_kafka_topic_destroy(*kafka_topic);
    rd_kafka_destroy(*producerHandler);

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;
    prepare_response_message(&returnMessage, "Kafka connection closed successfully\n");

    return returnMessage;
}

/*
 * Try to publish a message to a kafka queue and returns a message back. It is
 * possible for the returned KafkaStatusMessage.responseMessage to be NULL so
 * should be checked by the caller of this function.
 */
MetapublishStatusMessage kafka_write_message(rd_kafka_t **producerHandler, rd_kafka_topic_t **kafka_topic,
                                             GstBuffer *buffer) {
    gint msg;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = KAFKA;
    returnMessage.responseCode.kps = KAFKA_SUCCESS;

    if (*producerHandler == NULL) {
        returnMessage.responseCode.kps = KAFKA_ERROR;
        return returnMessage;
    }

    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (!jsonmeta) {
        returnMessage.responseCode.kps = KAFKA_ERROR_NO_INFERENCE;
        prepare_response_message(&returnMessage, "no json metadata found\n");
        return returnMessage;
    } else {
        msg = rd_kafka_produce(*kafka_topic, RD_KAFKA_PARTITION_UA, RD_KAFKA_MSG_F_COPY, jsonmeta->message,
                               strlen(jsonmeta->message), NULL, 0, NULL);
        if (msg == -1) {
            returnMessage.responseCode.kps = KAFKA_ERROR_NO_TOPIC_PRODUCED;
            prepare_response_message(&returnMessage, "Failed to produce to topic\n");
            return returnMessage;
        }
    }

    while (rd_kafka_outq_len(*producerHandler) > 0)
        rd_kafka_poll(*producerHandler, 0);

    prepare_response_message(&returnMessage, "Kafka message sent successfully\n");
    return returnMessage;
}

#endif
