/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include <gst/gst.h>
G_BEGIN_DECLS

typedef enum {
    SHORT_TERM,
    ZERO_TERM,
    SHORT_TERM_IMAGELESS,
    ZERO_TERM_IMAGELESS,
} GstGvaTrackingType;

#define GST_GVA_TRACKING_TYPE (gst_gva_get_tracking_type())
GType gst_gva_get_tracking_type(void);

G_END_DECLS
