/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __MQTTPUBLISHER_TYPES_H__
#define __MQTTPUBLISHER_TYPES_H__

#ifdef PAHO_INC
#include "MQTTClient.h"
#include "gva_json_meta.h"
#include "statusmessage.h"
#include <gst/gst.h>
typedef struct _MQTTPublishConfig {
    gchar *host;
    gchar *bindaddress;
    gchar *clientid;
    gchar *topic;
    gchar *timeout;
    gboolean signal_handoffs;
} MQTTPublishConfig;
#endif

#endif
