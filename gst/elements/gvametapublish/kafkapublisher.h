/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __KAFKAPUBLISHER_H__
#define __KAFKAPUBLISHER_H__

#include "gva_json_meta.h"
#include "statusmessage.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RESPONSE_MESSAGE 1024

#ifdef KAFKA_INC
#include "librdkafka/rdkafka.h"

typedef struct _KafkaPublishConfig KafkaPublishConfig;

struct _KafkaPublishConfig {
    gchar *address;
    gchar *topic;
    gboolean signal_handoffs;
};

MetapublishStatusMessage kafka_open_connection(KafkaPublishConfig *, rd_kafka_t **, rd_kafka_topic_t **);
MetapublishStatusMessage kafka_close_connection(rd_kafka_t **, rd_kafka_topic_t **);
MetapublishStatusMessage kafka_write_message(rd_kafka_t **, rd_kafka_topic_t **, GstBuffer *);
#endif

#endif
