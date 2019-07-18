/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __METAPUBLISHIMPL_H__
#define __METAPUBLISHIMPL_H__

#include "gstgvametapublish.h"
#ifdef PAHO_INC
#include "mqttpublisher.h"
#endif
#ifdef KAFKA_INC
#include "kafkapublisher.h"
#endif
#include "filepublisher.h"
#include "statusmessage.h"
#include <stdio.h>
#include <stdlib.h>

typedef enum { NONE, PUBLISH_FILE, PUBLISH_KAFKA, PUBLISH_MQTT } publishType;
typedef struct _MetapublishImpl MetapublishImpl;

struct _MetapublishImpl {
    publishType type;
// MQTT
#ifdef PAHO_INC
    MQTTPublishConfig *mqtt_config;
    MQTTClient mqtt_client;
#endif
// Kafka
#ifdef KAFKA_INC
    KafkaPublishConfig *kafka_config;
    rd_kafka_t *kafka_producerHandler;
    rd_kafka_topic_t *kafka_rkt;
#endif
    // File
    FilePublishConfig *file_config;
    FILE *pFile;
};

MetapublishImpl *getMPInstance();
void initializeMetaPublishImpl(GstGVAMetaPublishMethodType);

gint OpenConnection(GstGvaMetaPublish *);
gint CloseConnection(GstGvaMetaPublish *);
void WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf);

#endif /* __METAPUBLISHIMPL_H__ */
