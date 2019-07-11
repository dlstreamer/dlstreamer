/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "metapublish_impl.h"

MetapublishImpl *getMPInstance() {
    static MetapublishImpl *instance = NULL;
    if (instance == NULL) {
        instance = malloc(sizeof(*instance));
    }
    return instance;
};

gint OpenConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = getMPInstance();
#ifdef PAHO_INC
    if (mp->type == PUBLISH_MQTT) {
        mp->mqtt_config = g_try_malloc(sizeof(MQTTPublishConfig));
        if (mp->mqtt_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for MQTTPublishConfig");
            return -1;
        }
        mp->mqtt_config->host = gvametapublish->host;
        mp->mqtt_config->bindaddress = gvametapublish->address;
        mp->mqtt_config->clientid = gvametapublish->clientid;
        mp->mqtt_config->topic = gvametapublish->topic;
        mp->mqtt_config->timeout = gvametapublish->timeout;
        mp->mqtt_config->signal_handoffs = gvametapublish->signal_handoffs;

        mp->mqtt_client = mqtt_open_connection(mp->mqtt_config);
    }
#endif
#ifdef KAFKA_INC
    if (mp->type == PUBLISH_KAFKA) {
        mp->kafka_config = g_try_malloc(sizeof(KafkaPublishConfig));
        if (mp->kafka_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for KafkaPublishConfig");
            return -1;
        }
        mp->kafka_config->address = gvametapublish->address;
        mp->kafka_config->topic = gvametapublish->topic;
        mp->kafka_config->signal_handoffs = gvametapublish->signal_handoffs;

        kafka_open_connection(mp->kafka_config, mp->kafka_producerHandler, mp->kafka_rkt);
    }
#endif

    if (mp->type == PUBLISH_FILE) {
        mp->file_config = g_try_malloc(sizeof(FilePublishConfig));
        if (mp->file_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for FilePublishConfig");
            return -1;
        }
        mp->file_config->file_path = gvametapublish->file_path;

        // apply user specified override of output format
        if (!g_strcmp0(gvametapublish->output_format, STREAM)) {
            mp->file_config->e_output_format = FILE_PUBLISH_STREAM;
        } else {
            mp->file_config->e_output_format = FILE_PUBLISH_BATCH;
        }

        MetapublishStatusMessage status = file_open(mp->pFile, mp->file_config);
        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }

    return 0;
}

gint CloseConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = getMPInstance();
#ifdef PAHO_INC
    if (mp->type == PUBLISH_MQTT) {
        mqtt_close_connection(mp->mqtt_client);
        g_free(mp->mqtt_config);
    }
#endif

#ifdef KAFKA_INC
    if (mp->type == PUBLISH_KAFKA) {
        kafka_close_connection(mp->kafka_producerHandler, mp->kafka_rkt);
        g_free(mp->kafka_config);
    }
#endif

    if (mp->type == PUBLISH_FILE) {
        MetapublishStatusMessage status = file_close(mp->pFile, mp->file_config);
        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
        g_free(mp->file_config);
    }

    return 0;
}

void WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    MetapublishImpl *mp = getMPInstance();
#ifdef PAHO_INC
    if (mp->type == PUBLISH_MQTT) {
        MetapublishStatusMessage status = mqtt_write_message(mp->mqtt_client, mp->mqtt_config, buf);

        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
#endif
#ifdef KAFKA_INC
    if (mp->type == PUBLISH_KAFKA) {
        MetapublishStatusMessage status = kafka_write_message(mp->kafka_producerHandler, mp->kafka_rkt, buf);
        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
#endif

    if (mp->type == PUBLISH_FILE) {
        MetapublishStatusMessage status = file_write(mp->pFile, mp->file_config, buf);
        if (status.responseCode == -1) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
}
