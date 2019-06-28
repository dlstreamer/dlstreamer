/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "metapublish_impl.h"

MetapublishImpl* getMPInstance() {
    static MetapublishImpl *instance = NULL;
    if (instance == NULL) {
        instance = malloc(sizeof(*instance));
    }
    return instance;
};

gint OpenConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = getMPInstance();
    if (mp->type == PUBLISH_MQTT) {
        mp->mqtt_config = g_malloc(sizeof(MQTTPublishConfig));
        mp->mqtt_config->host = gvametapublish->host;
        mp->mqtt_config->bindaddress = gvametapublish->address;
        mp->mqtt_config->clientid = gvametapublish->clientid;
        mp->mqtt_config->topic = gvametapublish->topic;
        mp->mqtt_config->timeout = gvametapublish->timeout;
        mp->mqtt_config->signal_handoffs = gvametapublish->signal_handoffs;

        mp->mqtt_client = mqtt_open_connection(mp->mqtt_config);
    }
    
    return 0;
}

gint CloseConnection() {
    MetapublishImpl *mp = getMPInstance();
    if (mp->type == PUBLISH_MQTT) {
        mqtt_close_connection(mp->mqtt_client);
        //g_free(mp->mqtt_config);
    }
    return 0;
}

void WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    MetapublishImpl *mp = getMPInstance();
    if (mp->type == PUBLISH_MQTT) {
        MetapublishStatusMessage status = mqtt_write_message(mp->mqtt_client, mp->mqtt_config, buf);
        
        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
}
