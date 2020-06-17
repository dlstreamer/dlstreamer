/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __FILEPUBLISHER_TYPES_H__
#define __FILEPUBLISHER_TYPES_H__

// Common for any metapublish method (file, mqtt, kafka)
#include <gst/gst.h>
#include <stdio.h>

// File specific constants
#define JSON "json"
#define JSON_LINES "json-lines"
#define STDOUT "stdout"
#define MIN_FILE_LEN 4
#define JSON_RECORD_PREFIX ",\n"
#define JSON_LINES_RECORD_SUFFIX "\n"

typedef enum _tagPublishOutputFormat { FILE_PUBLISH_JSON_LINES = 0, FILE_PUBLISH_JSON = 1 } PublishOutputFormat;
typedef struct _FilePublishConfig {
    gchar *file_path;
    PublishOutputFormat e_file_format;
    gboolean signal_handoffs;
} FilePublishConfig;

#endif
