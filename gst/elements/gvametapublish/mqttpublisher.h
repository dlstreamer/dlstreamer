/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __MQTTPUBLISHER_H__
#define __MQTTPUBLISHER_H__

#include "gva_json_meta.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PAHO_INC
#include "MQTTClient.h"
typedef struct _MQTTPublishConfig MQTTPublishConfig;
typedef struct _MQTTStatusMessage MQTTStatusMessage;

struct _MQTTPublishConfig {
    gchar *host;
    gchar *bindaddress;
    gchar *clientid;
    gchar *topic;
    gchar *timeout;
    gboolean signal_handoffs;
};

struct _MQTTStatusMessage {
    gint responseCode;
    gchar *responseMessage;
};

MQTTStatusMessage mqtt_publish(MQTTPublishConfig *config, GstBuffer *buffer);
#endif

#endif
