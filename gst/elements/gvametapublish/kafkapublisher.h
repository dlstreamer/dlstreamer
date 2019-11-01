/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __KAFKAPUBLISHER_H__
#define __KAFKAPUBLISHER_H__

#ifdef KAFKA_INC
#include "kafkapublisher_types.h"
#define MAX_RESPONSE_MESSAGE 1024
MetapublishStatusMessage kafka_open_connection(KafkaPublishConfig *, rd_kafka_t **, rd_kafka_topic_t **);
MetapublishStatusMessage kafka_close_connection(rd_kafka_t **, rd_kafka_topic_t **);
MetapublishStatusMessage kafka_write_message(rd_kafka_t **, rd_kafka_topic_t **, GstBuffer *);
#endif

#endif
