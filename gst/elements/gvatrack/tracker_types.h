/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include <gst/gst.h>
G_BEGIN_DECLS

#ifdef ENABLE_TRACKER_TYPE_IOU
typedef enum { IOU, SHORT_TERM, ZERO_TERM, POSE } GstGvaTrackingType;
#else
typedef enum { SHORT_TERM, ZERO_TERM, POSE } GstGvaTrackingType;
#endif

#define GST_GVA_TRACKING_TYPE (gst_gva_get_tracking_type())
GType gst_gva_get_tracking_type(void);

G_END_DECLS
