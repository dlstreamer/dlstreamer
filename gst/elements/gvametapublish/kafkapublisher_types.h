/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __KAFKAPUBLISHER_TYPES_H__
#define __KAFKAPUBLISHER_TYPES_H__

#ifdef KAFKA_INC
#include "gva_json_meta.h"
#include "librdkafka/rdkafka.h"
#include "statusmessage.h"
#include <gst/gst.h>
typedef struct _KafkaPublishConfig {
    gchar *address;
    gchar *topic;
    gboolean signal_handoffs;
} KafkaPublishConfig;
#endif

#endif
