/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_types.h"

GType gst_gva_get_tracking_type(void) {
    static GType gva_tracking_type = 0;
    static const GEnumValue tracking_types[] = {{IOU, "IOU tracker", "iou"},
                                                {SHORT_TERM, "Short-term tracker", "short-term"},
                                                {ZERO_TERM, "Zero-term tracker", "zero-term"},
                                                {0, NULL, NULL}};

    if (!gva_tracking_type)
        gva_tracking_type = g_enum_register_static("GstGvaTrackingType", tracking_types);

    return gva_tracking_type;
}
