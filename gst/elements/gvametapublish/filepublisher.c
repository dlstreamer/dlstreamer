/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "filepublisher.h"
#define UNUSED(x) (void)(x)

// Caller is responsible to remove or rename existing inference file before processing
int do_initialize_file(FILE *pFile, const char *pathfile, const PublishOutputFormat eOutFormat) {
    pFile = fopen(pathfile, "r");
    if (!pFile) {
        pFile = fopen(pathfile, "w");
        if (pFile != NULL) {
            if (eOutFormat == FILE_PUBLISH_BATCH && ftell(pFile) <= 0) {
                fputs("[", pFile);
            }
        } else {
            return E_PUBLISH_ERROR_FILE_CREATE;
        }
    } else {
        return E_PUBLISH_ERROR_FILE_EXISTS;
    }
    return E_PUBLISH_SUCCESS;
}

int do_write_inference(FILE *pFile, const PublishOutputFormat eOutFormat, const gchar *inference) {
    if (pFile != NULL) {
        if (ftell(pFile) > 1) {
            if (eOutFormat == FILE_PUBLISH_BATCH) {
                fputs(",", pFile);
            }
            // Line feed for each record when producing either Stream or Batch
            fputs("\n", pFile);
        }
        fputs(inference, pFile);
    }
    return E_PUBLISH_SUCCESS;
}

int do_finalize_file(FILE *pFile, const PublishOutputFormat eOutFormat) {
    if (pFile != NULL) {
        if (eOutFormat == FILE_PUBLISH_BATCH && ftell(pFile) > 1) {
            fputs("]", pFile);
        }
        fclose(pFile);
    }
    return E_PUBLISH_SUCCESS;
}

MetapublishStatusMessage file_open(FILE *pFile, FilePublishConfig *config) {
    MetapublishStatusMessage returnMessage;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode = E_PUBLISH_ERROR;
        return returnMessage;
    }
    if (NULL == config->file_path || strlen(config->file_path) < MIN_FILE_LEN) {
        returnMessage.responseCode = E_PUBLISH_ERROR_INVALID_FILEPATH;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Error initializing file [%s] - You must specify absolute path not shorter than %d symbols to an "
                 "existing folder with the name of "
                 "output file.\n",
                 config->file_path, MIN_FILE_LEN);
        return returnMessage;
    }
    if (E_PUBLISH_SUCCESS != do_initialize_file(pFile, config->file_path, config->e_output_format)) {
        returnMessage.responseCode = E_PUBLISH_ERROR_FILE_EXISTS;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Error initializing file [%s]- remove or rename existing output file\n", config->file_path);
        return returnMessage;
    }
    returnMessage.responseCode = E_PUBLISH_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "File opened for write successfully\n");
    return returnMessage;
}

MetapublishStatusMessage file_close(FILE *pFile, FilePublishConfig *config) {
    MetapublishStatusMessage returnMessage;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode = E_PUBLISH_ERROR;
        return returnMessage;
    }
    if (E_PUBLISH_SUCCESS != do_finalize_file(pFile, config->e_output_format)) {
        returnMessage.responseCode = E_PUBLISH_ERROR_WRITING_FILE;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Error finalizing file\n");
        return returnMessage;
    }
    returnMessage.responseCode = E_PUBLISH_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "File completed successfully\n");
    return returnMessage;
}

MetapublishStatusMessage file_write(FILE *pFile, FilePublishConfig *config, GstBuffer *buffer) {
    MetapublishStatusMessage returnMessage;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode = E_PUBLISH_ERROR;
        return returnMessage;
    }
    returnMessage.responseCode = E_PUBLISH_ERROR;
    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (jsonmeta) {
        if (E_PUBLISH_SUCCESS != do_write_inference(pFile, config->e_output_format, jsonmeta->message)) {
            returnMessage.responseCode = E_PUBLISH_ERROR_WRITING_FILE;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Error writing inference to file\n");
            return returnMessage;
        }
    } else {
        returnMessage.responseCode = E_PUBLISH_ERROR_NO_INFERENCE;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "No json metadata to publish\n");
        return returnMessage;
    }
    returnMessage.responseCode = E_PUBLISH_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Message written successfully\n");
    return returnMessage;
}
