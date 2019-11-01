/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __FILEPUBLISHER_H__
#define __FILEPUBLISHER_H__
#include "filepublisher_types.h"
#include "statusmessage.h"

MetapublishStatusMessage file_open(FILE **pFile, FilePublishConfig *config);
MetapublishStatusMessage file_close(FILE **pFile, FilePublishConfig *config);
MetapublishStatusMessage file_write(FILE **pFile, FilePublishConfig *config, GstBuffer *buffer);

#endif
