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
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.ps = ERROR;
        return returnMessage;
    }

    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Failed to allocate memory for MetapublishImpl\n");
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
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Failed to allocate memory for MQTTPublishConfig\n");
            return returnMessage;
        }
        mp->mqtt_config->host = gvametapublish->host;
        mp->mqtt_config->bindaddress = gvametapublish->address;
        mp->mqtt_config->clientid = gvametapublish->clientid;
        mp->mqtt_config->topic = gvametapublish->topic;
        mp->mqtt_config->timeout = gvametapublish->timeout;
        mp->mqtt_config->signal_handoffs = gvametapublish->signal_handoffs;

        if (mp->mqtt_config->bindaddress == NULL) {
            returnMessage.responseCode.ps = ERROR;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Failed to Open MQTT Connection, No Address provided\n");
            return returnMessage;
        }

        if (mp->mqtt_config->topic == NULL) {
            returnMessage.responseCode.ps = ERROR;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Failed to Open MQTT Connection, No Topic provided\n");
            return returnMessage;
        }

        if (mp->mqtt_config->timeout == NULL) {
            mp->mqtt_config->timeout = "1000";
        }

        mp->mqtt_client = mqtt_open_connection(mp->mqtt_config);

        if (mp->mqtt_client == NULL) {
            returnMessage.responseCode.ps = ERROR;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to Open MQTT Connection\n");
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
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Failed to allocate memory for KafkaPublishConfig\n");
            return returnMessage;
        }
        mp->kafka_config->address = gvametapublish->address;
        mp->kafka_config->topic = gvametapublish->topic;
        mp->kafka_config->signal_handoffs = gvametapublish->signal_handoffs;

        MetapublishStatusMessage status =
            kafka_open_connection(mp->kafka_config, &mp->kafka_producerHandler, &mp->kafka_rkt);
        if (status.responseCode.kps != KAFKA_SUCCESS) {
            returnMessage.responseCode.ps = ERROR;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to open Kafka Connection\n");
            g_free(status.responseMessage);
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
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Failed to allocate memory for FilePublishConfig\n");
            return returnMessage;
        }
        mp->file_config->file_path = gvametapublish->file_path;

        // apply user specified override of output format
        if (!g_strcmp0(gvametapublish->output_format, STREAM)) {
            mp->file_config->e_output_format = FILE_PUBLISH_STREAM;
        } else {
            mp->file_config->e_output_format = FILE_PUBLISH_BATCH;
        }

        MetapublishStatusMessage status = file_open(&mp->pFile, mp->file_config);
        if (status.responseCode.fps != FILE_SUCCESS) {
            GST_ERROR_OBJECT(gvametapublish, "%s", status.responseMessage);
            GST_ELEMENT_ERROR(gvametapublish, RESOURCE, TOO_LAZY, ("metapublish initialization failed"),
                              ("%s", status.responseMessage));
            returnMessage.responseCode.ps = ERROR;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to open File\n");
            g_free(status.responseMessage);
            return returnMessage;
        } else {
            GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
            g_free(status.responseMessage);
        }
    }

    returnMessage.responseCode.ps = SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "MetaPublish Target Opened Successfully\n");
    return returnMessage;
}

MetapublishStatusMessage CloseConnection(GstGvaMetaPublish *gvametapublish) {
    MetapublishImpl *mp = &gvametapublish->instance_impl;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = GENERAL;
    returnMessage.responseCode.ps = SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.ps = ERROR;
        return returnMessage;
    }
    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Failed to allocate memory for MetapublishImpl\n");
        return returnMessage;
    }

    MetapublishStatusMessage status;
    status.responseMessage = NULL;
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
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Failed to close connection\n");
    } else {
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Close Connection Successful\n");
    }
    if (status.responseMessage != NULL) {
        g_free(status.responseMessage);
    }
    return returnMessage;
}

MetapublishStatusMessage WriteMessage(GstGvaMetaPublish *gvametapublish, GstBuffer *buf) {
    MetapublishImpl *mp = &gvametapublish->instance_impl;
    MetapublishStatusMessage status;
    status.responseMessage = NULL;
    status.codeType = GENERAL;
    status.responseCode.ps = SUCCESS;

    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = GENERAL;
    returnMessage.responseCode.ps = SUCCESS;
    returnMessage.responseMessage = (gchar *)g_try_malloc(MAX_RESPONSE_MESSAGE);

    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.ps = ERROR;
        return returnMessage;
    }

    if (mp == NULL) {
        returnMessage.responseCode.ps = ERROR;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Failed to allocate memory for MetapublishImpl\n");
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
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Error during publish metadata\n");
    } else {
        GST_INFO_OBJECT(gvametapublish, "%s", status.responseMessage);
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Publish data successful\n");
    }
    if (status.responseMessage != NULL) {
        g_free(status.responseMessage);
    }

    return returnMessage;
}
