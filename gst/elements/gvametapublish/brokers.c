/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "brokers.h"

// Helper to translate generic GstGvaMetaPublish properties to file-specific configuration
// NOTE: Caller responsible to allocate and free the populated config struct
gboolean get_file_config(GstGvaMetaPublish *gvametapublish, FilePublishConfig *config) {
    if (config) {
        config->file_path = gvametapublish->file_path;
        config->e_output_format = E_OF_BATCH; // DEFAULT_OUTPUT_FORMAT
        // apply user specified override of output format
        if (!g_strcmp0(gvametapublish->output_format, OF_STREAM)) {
            config->e_output_format = E_OF_STREAM;
        }
    }
    return TRUE;
}

gboolean initialize_file(GstGvaMetaPublish *gvametapublish) {
    gboolean result = TRUE;
    FilePublishConfig config;
    if (get_file_config(gvametapublish, &config)) {
        FileStatusMessage status = file_publish_initialize(&config);
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        if (status.responseCode < 0) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
            result = FALSE;
        }
        g_free(status.responseMessage);
    }
    return result;
}

void finalize_file(GstGvaMetaPublish *gvametapublish) {
    FilePublishConfig config;
    if (get_file_config(gvametapublish, &config)) {
        FileStatusMessage status = file_publish_finalize(&config);
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        if (status.responseCode < 0) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
}

void publish_file(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    FilePublishConfig config;
    if (get_file_config(gvametapublish, &config)) {
        FileStatusMessage status = file_publish(&config, buf);
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        if (status.responseCode < 0) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
        g_free(status.responseMessage);
    }
}

#ifdef KAFKA_INC
void publish_kafka(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    KafkaPublishConfig *config;

    config = g_try_malloc(sizeof(KafkaPublishConfig));
    if (config == NULL) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for KafkaPublishConfig");
        return;
    }

    config->address = gvametapublish->address;
    config->topic = gvametapublish->topic;
    config->signal_handoffs = gvametapublish->signal_handoffs;

    KafkaStatusMessage status = kafka_publish(config, buf);
    g_free(config);

    if (status.responseMessage == NULL) {
        GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for KafkaStatusMessage.responseMessage");
        return;
    }

    if (status.responseCode == -1) {
        GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
    } else {
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
    }

    g_free(status.responseMessage);
}
#endif

#ifdef PAHO_INC
void publish_mqtt(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    MQTTPublishConfig *config;

    config = g_malloc(sizeof(MQTTPublishConfig));
    config->host = gvametapublish->host;
    config->bindaddress = gvametapublish->address;
    config->clientid = gvametapublish->clientid;
    config->topic = gvametapublish->topic;
    config->timeout = gvametapublish->timeout;
    config->signal_handoffs = gvametapublish->signal_handoffs;

    MQTTStatusMessage status = mqtt_publish(config, buf);
    g_free(config);

    GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
    if (status.responseCode == -1) {
        GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
    }
    g_free(status.responseMessage);
}
#endif

BrokerMap brokers[] = {{"file", initialize_file, publish_file, finalize_file},
#ifdef KAFKA_INC
                       {"kafka", NULL, publish_kafka, NULL},
#endif
#ifdef PAHO_INC
                       {"mqtt", NULL, publish_mqtt, NULL},
#endif
                       {NULL, NULL, NULL, NULL}};

const int lengthOfBrokers = sizeof(brokers) / sizeof(brokers[0]);
