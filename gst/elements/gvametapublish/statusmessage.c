/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "statusmessage.h"

void prepare_response_message(MetapublishStatusMessage *message, const gchar *responseMessage) {
    g_assert(message != NULL);
    g_assert(responseMessage != NULL);
    g_strlcpy(message->responseMessage, responseMessage, sizeof(message->responseMessage));
}
