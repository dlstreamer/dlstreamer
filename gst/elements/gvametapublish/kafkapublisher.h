/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __KAFKAPUBLISHER_H__
#define __KAFKAPUBLISHER_H__

#include "gva_json_meta.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef KAFKA_INC
#include "librdkafka/rdkafka.h"

typedef struct _KafkaPublishConfig KafkaPublishConfig;
typedef struct _KafkaStatusMessage KafkaStatusMessage;

struct _KafkaPublishConfig {
    gchar *address;
    gchar *topic;
    gboolean signal_handoffs;
};

struct _KafkaStatusMessage {
    gint responseCode;
    gchar *responseMessage;
};

KafkaStatusMessage kafka_publish(KafkaPublishConfig *config, GstBuffer *buffer);
#endif

#endif
