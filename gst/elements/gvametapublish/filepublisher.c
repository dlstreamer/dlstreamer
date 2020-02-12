/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "filepublisher.h"
#include "gva_json_meta.h"
#define UNUSED(x) (void)(x)

// NOTE: Caller is responsible to remove or rename existing inference file before
//  processing when output_format=Batch
/* TODO: Confirm that closing stdout on first of many pipelines (running within
    the same process) does not impact subsequent pipeline output to stdout.
*/

FilePublishStatus do_initialize_file(FILE **pFile, const char *pathfile, const PublishOutputFormat eOutFormat) {
    if (0 == strcmp(pathfile, STDOUT)) {
        *pFile = stdout;
    } else if (eOutFormat == FILE_PUBLISH_STREAM) {
        *pFile = fopen(pathfile, "a+");
        if (*pFile == NULL) {
            return FILE_ERROR_FILE_CREATE;
        }
        if (setvbuf(*pFile, NULL, _IOLBF, 0)) {
            return FILE_ERROR_INITIALIZING_BUFFER;
        }
    } else { // FILE_PUBLISH_BATCH
        *pFile = fopen(pathfile, "w+");
    }
    if (*pFile == NULL) {
        return FILE_ERROR_FILE_CREATE;
    }
    if (eOutFormat == FILE_PUBLISH_BATCH) {
        fputs("[", *pFile);
    }
    return FILE_SUCCESS;
}

static inline void write_message_prefix(FILE *pFile, const PublishOutputFormat eOutFormat) {
    // Add comma and line feed before the record when producing a Batch
    if (eOutFormat == FILE_PUBLISH_BATCH) {
        static gboolean need_line_break = FALSE;
        if (!need_line_break) {
            if (ftello(pFile) > 2) {
                need_line_break = TRUE;
            }
        }
        if (need_line_break) {
            // a prior record was written, precede this message with record separator
            fputs(BATCH_RECORD_PREFIX, pFile);
        }
    }
}
static inline void write_message_suffix(FILE *pFile, const PublishOutputFormat eOutFormat) {
    // Add line feed after each record when producing a Stream file/FIFO
    if (eOutFormat == FILE_PUBLISH_STREAM) {
        fputs(STREAM_RECORD_SUFFIX, pFile);
    }
}

FilePublishStatus do_write_message(FILE **pFile, const PublishOutputFormat eOutFormat, const gchar *inference_message) {
    if (*pFile) {
        write_message_prefix(*pFile, eOutFormat);
        fputs(inference_message, *pFile);
        write_message_suffix(*pFile, eOutFormat);
    } else {
        return FILE_ERROR;
    }
    return FILE_SUCCESS;
}

FilePublishStatus do_finalize_file(FILE **pFile, const char *pathfile, const PublishOutputFormat eOutFormat) {
    if (*pFile != NULL) {
        if (eOutFormat == FILE_PUBLISH_BATCH && ftello(*pFile) > 2) {
            fputs("]", *pFile);
        }
        fputs("\n", *pFile);
        // For any pathfile we initialized w/ fopen(), invoke corresponding fclose()
        if (0 != strcmp(pathfile, STDOUT)) {
            if (0 != fclose(*pFile)) {
                return FILE_ERROR;
            }
        }
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
    FilePublishStatus fps = do_initialize_file(pFile, config->file_path, config->e_output_format);
    if (fps != FILE_SUCCESS) {
        switch (fps) {
        case FILE_ERROR_FILE_EXISTS:
            snprintf(
                returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                "Error initializing file %s- existing output file must be removed or renamed to avoid data loss.\n",
                config->file_path);
            break;
        case FILE_ERROR_FILE_CREATE:
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Error initializing file %s- could not open requested file with write permissions. Check user "
                     "access to file system.\n",
                     config->file_path);
            break;
        case FILE_ERROR_INITIALIZING_BUFFER:
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Error initializing buffering for file stream %s.\n", config->file_path);
            break;
        default:
            snprintf(returnMessage.responseMessage, MAX_RESPONSE_MESSAGE,
                     "Error initializing file %s- an unexpected condition occurred during output file initialization. "
                     "Check user access to file system.\n",
                     config->file_path);
            break;
        }
        returnMessage.responseCode.fps = fps;
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
    FilePublishStatus status = do_finalize_file(pFile, config->file_path, config->e_output_format);
    if (status != FILE_SUCCESS) {
        returnMessage.responseCode.fps = status;
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
        FilePublishStatus status = do_write_message(pFile, config->e_output_format, jsonmeta->message);
        if (status != FILE_SUCCESS) {
            returnMessage.responseCode.fps = status;
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
