/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef __FILEPUBLISHER_TYPES_H__
#define __FILEPUBLISHER_TYPES_H__

// Common for any metapublish method (file, mqtt, kafka)
#include <gst/gst.h>
#include <stdio.h>

// File specific constants
#define BATCH "batch"
#define STREAM "stream"
#define STDOUT "stdout"
#define MIN_FILE_LEN 4
#define BATCH_RECORD_PREFIX ",\n"
#define STREAM_RECORD_SUFFIX "\n"

typedef enum _tagPublishOutputFormat { FILE_PUBLISH_STREAM = 0, FILE_PUBLISH_BATCH = 1 } PublishOutputFormat;
typedef struct _FilePublishConfig {
    gchar *file_path;
    PublishOutputFormat e_output_format;
    gboolean signal_handoffs;
} FilePublishConfig;

#endif
