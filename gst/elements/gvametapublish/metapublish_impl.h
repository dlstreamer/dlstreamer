/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __METAPUBLISHIMPL_H__
#define __METAPUBLISHIMPL_H__

#include "gstgvametapublish.h"
#include "mqttpublisher.h"
#include <stdio.h>
#include <stdlib.h>
#include "statusmessage.h"

typedef enum { NONE, PUBLISH_FILE, PUBLISH_KAFKA, PUBLISH_MQTT } publishType;
typedef struct _MetapublishImpl MetapublishImpl;

struct _MetapublishImpl {
    publishType type;
    //MQTT
    MQTTClient mqtt_client;
    MQTTPublishConfig *mqtt_config;
};

MetapublishImpl* getMPInstance();


gint OpenConnection(GstGvaMetaPublish*);
gint CloseConnection();
void WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf);


#endif /* __METAPUBLISHIMPL_H__ */
