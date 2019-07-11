/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __STATUSMESSAGE_H__
#define __STATUSMESSAGE_H__

#include <stdio.h>
#include <stdlib.h>

typedef struct _MetapublishStatusMessage MetapublishStatusMessage;

struct _MetapublishStatusMessage {
    gint responseCode;
    gchar *responseMessage;
};

#endif