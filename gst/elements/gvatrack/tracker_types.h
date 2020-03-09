/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

typedef enum { IOU, SHORT_TERM, ZERO_TERM } GstGvaTrackingType;

#define GST_GVA_TRACKING_TYPE (gst_gva_get_tracking_type())
GType gst_gva_get_tracking_type(void);

G_END_DECLS
