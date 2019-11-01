/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __MQTTPUBLISHER_H__
#define __MQTTPUBLISHER_H__

#ifdef PAHO_INC
#include "mqttpublisher_types.h"
MQTTClient mqtt_open_connection(MQTTPublishConfig *gvametapublish);
MetapublishStatusMessage mqtt_close_connection(MQTTClient client);
MetapublishStatusMessage mqtt_write_message(MQTTClient client, MQTTPublishConfig *gvametapublish, GstBuffer *buffer);
#endif

#endif
