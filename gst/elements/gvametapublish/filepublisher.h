/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __FILEPUBLISHER_H__
#define __FILEPUBLISHER_H__

#include "gva_json_meta.h"
#include "statusmessage.h"
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// File specific constants
typedef enum _tagPublishOutputFormat { FILE_PUBLISH_STREAM = 0, FILE_PUBLISH_BATCH = 1 } PublishOutputFormat;

typedef struct _FilePublishConfig FilePublishConfig;

struct _FilePublishConfig {
    gchar *file_path;
    PublishOutputFormat e_output_format;
    gboolean signal_handoffs;
};

#define BATCH "batch"
#define STREAM "stream"
#define STDOUT "stdout"
#define MIN_FILE_LEN 4

MetapublishStatusMessage file_open(FILE **pFile, FilePublishConfig *config);
MetapublishStatusMessage file_close(FILE **pFile, FilePublishConfig *config);
MetapublishStatusMessage file_write(FILE **pFile, FilePublishConfig *config, GstBuffer *buffer);

#endif
