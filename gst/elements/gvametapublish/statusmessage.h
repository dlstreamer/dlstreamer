/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __STATUSMESSAGE_H__
#define __STATUSMESSAGE_H__

#include <stdio.h>
#include <stdlib.h>

#define MAX_RESPONSE_MESSAGE 1024

typedef struct _MetapublishStatusMessage MetapublishStatusMessage;

typedef enum _PublishStatusType { MQTT, KAFKA, FILESTATUS, GENERAL } PublishStatusType;

typedef enum _publish_error { SUCCESS = 0, ERROR = -1 } PublishStatus;

typedef enum _mqtt_publish_error {
    MQTT_SUCCESS = 0,
    MQTT_ERROR = -1,
    MQTT_ERROR_NO_CONNECTION = -2,
    MQTT_ERROR_NO_INFERENCE = -3
} MQTTPublishStatus;

typedef enum _kafka_publish_error {
    KAFKA_SUCCESS = 0,
    KAFKA_ERROR = -1,
    KAFKA_ERROR_NO_INFERENCE = -2,
    KAFKA_ERROR_NO_TOPIC_PRODUCED = -3
} KafkaPublishStatus;

typedef enum _file_publish_error {
    FILE_SUCCESS = 0,
    FILE_ERROR = -1,
    FILE_ERROR_WRITING_FILE = -2,
    FILE_ERROR_NO_INFERENCE = -3,
    FILE_ERROR_FILE_EXISTS = -4,
    FILE_ERROR_FILE_CREATE = -5,
    FILE_ERROR_INVALID_FILEPATH = -6
} FilePublishStatus;

struct _MetapublishStatusMessage {
    PublishStatusType codeType;
    union ResponseCode {
        PublishStatus ps;
        FilePublishStatus fps;
        MQTTPublishStatus mps;
        KafkaPublishStatus kps;
    } responseCode;
    gchar *responseMessage;
};

#endif