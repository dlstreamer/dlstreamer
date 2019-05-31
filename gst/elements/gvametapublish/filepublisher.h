/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __FILEPUBLISHER_H__
#define __FILEPUBLISHER_H__

#include "gva_json_meta.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// File specific constants
typedef enum _tagPublishOutputFormat { E_OF_STREAM = 0, E_OF_BATCH = 1 } PublishOutputFormat;

typedef struct _FilePublishConfig {
    gchar *file_path;
    PublishOutputFormat e_output_format;
    gboolean signal_handoffs;
} FilePublishConfig;

#define OF_BATCH "batch"
#define OF_STREAM "stream"
#define MAX_RESPONSE_MESSAGE 1024
#define MIN_FILE_LEN 4

// TODO: Convert this FileStatusMessage to map an array of errdesc elements (with static strings).
//      This way APIs simply supply the enum (reduce boilerplate)
typedef struct _FileStatusMessage {
    gint responseCode;
    gchar *responseMessage;
} FileStatusMessage;

FileStatusMessage file_publish(FilePublishConfig *config, GstBuffer *buffer);
FileStatusMessage file_publish_finalize(FilePublishConfig *config);
FileStatusMessage file_publish_initialize(FilePublishConfig *config);

enum _publish_error {
    E_PUBLISH_SUCCESS = 0,
    E_PUBLISH_ERROR = -1,
    E_PUBLISH_ERROR_WRITING_FILE = -2,
    E_PUBLISH_ERROR_NO_INFERENCE = -3,
    E_PUBLISH_ERROR_FILE_EXISTS = -4,
    E_PUBLISH_ERROR_FILE_CREATE = -5,
    E_PUBLISH_ERROR_INVALID_FILEPATH = -6
};

#endif
