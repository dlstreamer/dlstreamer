/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __MQTTPUBLISHER_H__
#define __MQTTPUBLISHER_H__

#include "gva_json_meta.h"
#include "statusmessage.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PAHO_INC
#include "MQTTClient.h"
typedef struct _MQTTPublishConfig MQTTPublishConfig;

struct _MQTTPublishConfig {
    gchar *host;
    gchar *bindaddress;
    gchar *clientid;
    gchar *topic;
    gchar *timeout;
    gboolean signal_handoffs;
};

MQTTClient mqtt_open_connection(MQTTPublishConfig *gvametapublish);
void mqtt_close_connection(MQTTClient client);
MetapublishStatusMessage mqtt_write_message(MQTTClient client, MQTTPublishConfig *gvametapublish, GstBuffer *buffer);

#endif

#endif
