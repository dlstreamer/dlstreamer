/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

enum GvaBufferFlags {
    GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME = (GST_BUFFER_FLAG_LAST << 1),
    GST_BUFFER_FLAG_READY_TO_PUSH = (GST_BUFFER_FLAG_LAST << 2)
};

enum GvaQueryTypes {
    GVA_QUERY_MODEL_INPUT = GST_QUERY_MAKE_TYPE(500, GST_QUERY_TYPE_DOWNSTREAM),
    GVA_QUERY_MODEL_OUTPUT = GST_QUERY_MAKE_TYPE(501, GST_QUERY_TYPE_UPSTREAM),
    GVA_QUERY_MODEL_INFO = GST_QUERY_MAKE_TYPE(502, GST_QUERY_TYPE_BOTH)
};

enum GvaEventTypes {
    GVA_EVENT_PREPROC_INFO = GST_QUERY_MAKE_TYPE(700, GST_EVENT_TYPE_STICKY | GST_EVENT_TYPE_DOWNSTREAM)
};
