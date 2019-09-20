/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "filepublisher.h"
#define UNUSED(x) (void)(x)

// Caller is responsible to remove or rename existing inference file before processing

static inline gboolean need_line_separator(FILE *pFile, const PublishOutputFormat eOutFormat) {
    return ((ftell(pFile) > 1 && eOutFormat == FILE_PUBLISH_BATCH) || pFile == stdout);
}
FilePublishStatus do_initialize_file(FILE **pFile, const char *pathfile, const PublishOutputFormat eOutFormat) {
    int result = strcmp(pathfile, "stdout");
    if (result == 0) {
        *pFile = stdout;
        fputs("[", *pFile);
        return FILE_SUCCESS;
    }
    *pFile = fopen(pathfile, "r");
    if (*pFile == NULL) {
        *pFile = fopen(pathfile, "w+");
        if (*pFile != NULL) {
            if (eOutFormat == FILE_PUBLISH_BATCH && ftell(*pFile) <= 0) {
                fputs("[", *pFile);
            }
        } else {
            return FILE_ERROR_FILE_CREATE;
        }
    } else {
        return FILE_ERROR_FILE_EXISTS;
    }
    return FILE_SUCCESS;
}

FilePublishStatus do_write_inference(FILE **pFile, const PublishOutputFormat eOutFormat, const gchar *inference) {
    if (*pFile != NULL) {
        if (need_line_separator(*pFile, eOutFormat)) {
            fputs(",", *pFile);
            // Line feed for each record when producing either Stream or Batch
            fputs("\n", *pFile);
        }
        fputs(inference, *pFile);
    } else {
        return FILE_ERROR;
    }
    return FILE_SUCCESS;
}

FilePublishStatus do_finalize_file(FILE **pFile, const PublishOutputFormat eOutFormat) {
    if (*pFile != NULL) {
        if (need_line_separator(*pFile, eOutFormat)) {
            fputs("]", *pFile);
        }
        fclose(*pFile);
    } else {
        return FILE_ERROR;
    }
    return FILE_SUCCESS;
}

MetapublishStatusMessage file_open(FILE **pFile, FilePublishConfig *config) {
    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = FILESTATUS;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.fps = FILE_ERROR;
        return returnMessage;
    }
    if (config->file_path == NULL) {
        returnMessage.responseCode.fps = FILE_ERROR_INVALID_FILEPATH;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "filepath property for gvametapublish has not been set\n");
        return returnMessage;
    } else if (strlen(config->file_path) < MIN_FILE_LEN) {
        returnMessage.responseCode.fps = FILE_ERROR_INVALID_FILEPATH;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Error initializing file %s - You must specify absolute path not shorter than %d symbols to an "
                 "existing folder with the name of "
                 "output file.\n",
                 config->file_path, MIN_FILE_LEN);
        return returnMessage;
    }
    if (do_initialize_file(pFile, config->file_path, config->e_output_format) != FILE_SUCCESS) {
        returnMessage.responseCode.fps = FILE_ERROR_FILE_EXISTS;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                 "Error initializing file %s- remove or rename existing output file\n", config->file_path);
        return returnMessage;
    }
    returnMessage.responseCode.fps = FILE_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "File opened for write successfully\n");
    return returnMessage;
}

MetapublishStatusMessage file_close(FILE **pFile, FilePublishConfig *config) {
    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = FILESTATUS;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.fps = FILE_ERROR;
        return returnMessage;
    }
    FilePublishStatus fpe = do_finalize_file(pFile, config->e_output_format);
    if (fpe != FILE_SUCCESS) {
        returnMessage.responseCode.fps = fpe;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Error finalizing file\n");
        return returnMessage;
    }
    returnMessage.responseCode.fps = FILE_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "File completed successfully\n");
    return returnMessage;
}

MetapublishStatusMessage file_write(FILE **pFile, FilePublishConfig *config, GstBuffer *buffer) {
    MetapublishStatusMessage returnMessage;
    returnMessage.codeType = FILESTATUS;
    returnMessage.responseMessage = (gchar *)malloc(MAX_RESPONSE_MESSAGE);
    if (returnMessage.responseMessage == NULL) {
        returnMessage.responseCode.fps = FILE_ERROR;
        return returnMessage;
    }
    returnMessage.responseCode.fps = FILE_ERROR;
    GstGVAJSONMeta *jsonmeta = GST_GVA_JSON_META_GET(buffer);
    if (jsonmeta) {
        FilePublishStatus fpe = do_write_inference(pFile, config->e_output_format, jsonmeta->message);
        if (fpe != FILE_SUCCESS) {
            returnMessage.responseCode.fps = fpe;
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Error writing inference to file\n");
            return returnMessage;
        }
    } else {
        returnMessage.responseCode.fps = FILE_ERROR_NO_INFERENCE;
        snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "No json metadata to publish\n");
        return returnMessage;
    }
    returnMessage.responseCode.fps = FILE_SUCCESS;
    snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE, "Message written successfully\n");
    return returnMessage;
}
