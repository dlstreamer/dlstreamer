/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "gstgvametapublish.h"

#include "metapublish_impl.h"

MetapublishStatusMessage OpenConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = &gvametapublish->instance_impl;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = GENERAL;
    returnMessage.responseCode.ps = SUCCESS;

    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        prepare_response_message(&returnMessage, "Failed to allocate memory for MetapublishImpl\n");
        return returnMessage;
    }

#ifdef PAHO_INC
    if (mp->type == GST_GVA_METAPUBLISH_MQTT) {
        mp->mqtt_config = g_try_malloc(sizeof(MQTTPublishConfig));
        if (mp->mqtt_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for MQTTPublishConfig");
            GST_ELEMENT_ERROR(gvametapublish, RESOURCE, TOO_LAZY, ("metapublish initialization failed"),

                              ("Failed to allocate memory for mqtt config"));
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to allocate memory for MQTTPublishConfig\n");
            return returnMessage;
        }
        mp->mqtt_config->address = gvametapublish->address;
        mp->mqtt_config->clientid = gvametapublish->mqtt_client_id;
        mp->mqtt_config->topic = gvametapublish->topic;
        mp->mqtt_config->timeout = gvametapublish->timeout;
        mp->mqtt_config->signal_handoffs = gvametapublish->signal_handoffs;

        if (mp->mqtt_config->address == NULL) {
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to Open MQTT Connection, No Address provided\n");
            return returnMessage;
        }

        if (mp->mqtt_config->topic == NULL) {
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to Open MQTT Connection, No Topic provided\n");
            return returnMessage;
        }

        if (mp->mqtt_config->timeout == NULL) {
            mp->mqtt_config->timeout = "1000";
        }

        mp->mqtt_client = mqtt_open_connection(mp->mqtt_config);

        if (mp->mqtt_client == NULL) {
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to Open MQTT Connection\n");
            return returnMessage;
        }
    }
#endif
#ifdef KAFKA_INC
    if (mp->type == GST_GVA_METAPUBLISH_KAFKA) {
        mp->kafka_config = g_try_malloc(sizeof(KafkaPublishConfig));
        if (mp->kafka_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for KafkaPublishConfig");
            GST_ELEMENT_ERROR(gvametapublish, RESOURCE, TOO_LAZY, ("metapublish initialization failed"),
                              ("Failed to allocate memory for kafka config"));
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to allocate memory for KafkaPublishConfig\n");
            return returnMessage;
        }
        mp->kafka_config->address = gvametapublish->address;
        mp->kafka_config->topic = gvametapublish->topic;
        mp->kafka_config->signal_handoffs = gvametapublish->signal_handoffs;

        MetapublishStatusMessage status =
            kafka_open_connection(mp->kafka_config, &mp->kafka_producerHandler, &mp->kafka_rkt);
        if (status.responseCode.kps != KAFKA_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to open Kafka Connection\n");
            return returnMessage;
        }
    }
#endif

    if (mp->type == GST_GVA_METAPUBLISH_FILE) {
        mp->file_config = g_try_malloc(sizeof(FilePublishConfig));
        if (mp->file_config == NULL) {
            GST_ERROR_OBJECT(gvametapublish, "Failed to allocate memory for FilePublishConfig");
            GST_ELEMENT_ERROR(gvametapublish, RESOURCE, FAILED, ("metapublish initialization failed"),
                              ("Failed to allocate memory for file config"));
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to allocate memory for FilePublishConfig\n");
            return returnMessage;
        }
        mp->file_config->file_path = gvametapublish->file_path;

        // apply user specified override of file format
        if (!g_strcmp0(gvametapublish->file_format, JSON_LINES)) {
            mp->file_config->e_file_format = FILE_PUBLISH_JSON_LINES;
        } else {
            mp->file_config->e_file_format = FILE_PUBLISH_JSON;
        }

        MetapublishStatusMessage status = file_open(&mp->pFile, mp->file_config);
        if (status.responseCode.fps != FILE_SUCCESS) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
            GST_ELEMENT_ERROR(gvametapublish, RESOURCE, TOO_LAZY, ("metapublish initialization failed"),
                              ("%s", status.responseMessage));
            returnMessage.responseCode.ps = ERROR;
            prepare_response_message(&returnMessage, "Failed to open File\n");
            return returnMessage;
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        }
    }

    returnMessage.responseCode.ps = SUCCESS;
    prepare_response_message(&returnMessage, "MetaPublish Target Opened Successfully\n");
    return returnMessage;
}

MetapublishStatusMessage CloseConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = &gvametapublish->instance_impl;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = GENERAL;
    returnMessage.responseCode.ps = SUCCESS;

    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        prepare_response_message(&returnMessage, "Failed to allocate memory for MetapublishImpl\n");
        return returnMessage;
    }

    MetapublishStatusMessage status;
    status.codeType = GENERAL;
    status.responseCode.ps = SUCCESS;

#ifdef PAHO_INC
    if (mp->type == GST_GVA_METAPUBLISH_MQTT) {
        status = mqtt_close_connection(mp->mqtt_client);
        g_free(mp->mqtt_config);
    }
#endif
#ifdef KAFKA_INC
    if (mp->type == GST_GVA_METAPUBLISH_KAFKA) {
        status = kafka_close_connection(&mp->kafka_producerHandler, &mp->kafka_rkt);
        g_free(mp->kafka_config);
    }
#endif
    if (mp->type == GST_GVA_METAPUBLISH_FILE) {
        status = file_close(&mp->pFile, mp->file_config);
        g_free(mp->file_config);
    }

    switch (status.codeType) {
    case MQTT:
        if (status.responseCode.mps != MQTT_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    case KAFKA:
        if (status.responseCode.kps != KAFKA_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    case FILESTATUS:
        if (status.responseCode.fps != FILE_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    default:
        returnMessage.responseCode.ps = ERROR;
        break;
    }

    if (returnMessage.responseCode.ps != SUCCESS) {
        GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        prepare_response_message(&returnMessage, "Failed to close connection\n");
    } else {
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        prepare_response_message(&returnMessage, "Close Connection Successful\n");
    }
    return returnMessage;
}

MetapublishStatusMessage WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    MetapublishImpl *mp = &gvametapublish->instance_impl;
    MetapublishStatusMessage status;
    status.codeType = GENERAL;
    status.responseCode.ps = SUCCESS;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = GENERAL;
    returnMessage.responseCode.ps = SUCCESS;

    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        prepare_response_message(&returnMessage, "Failed to allocate memory for MetapublishImpl\n");
        return returnMessage;
    }

#ifdef PAHO_INC
    if (mp->type == GST_GVA_METAPUBLISH_MQTT) {
        status = mqtt_write_message(mp->mqtt_client, mp->mqtt_config, buf);
    }
#endif
#ifdef KAFKA_INC
    if (mp->type == GST_GVA_METAPUBLISH_KAFKA) {
        status = kafka_write_message(&mp->kafka_producerHandler, &mp->kafka_rkt, buf);
    }
#endif
    if (mp->type == GST_GVA_METAPUBLISH_FILE) {
        status = file_write(&mp->pFile, mp->file_config, buf);
    }

    switch (status.codeType) {
    case MQTT:
        if (status.responseCode.mps != MQTT_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    case KAFKA:
        if (status.responseCode.kps != KAFKA_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    case FILESTATUS:
        if (status.responseCode.fps != FILE_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
        }
        break;
    default:
        returnMessage.responseCode.ps = ERROR;
        break;
    }

    if (returnMessage.responseCode.ps != SUCCESS) {
        GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
        prepare_response_message(&returnMessage, "Error during publish metadata\n");
    } else {
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        prepare_response_message(&returnMessage, "Publish data successful\n");
    }

    return returnMessage;
}
