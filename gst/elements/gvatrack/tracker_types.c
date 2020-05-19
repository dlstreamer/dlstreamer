/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_types.h"

GType gst_gva_get_tracking_type(void) {
    static GType gva_tracking_type = 0;
#ifdef ENABLE_TRACKER_TYPE_IOU

    static const GEnumValue tracking_types[] = {{IOU, "IOU Intersection Over Union tracker", "iou"},
                                                {SHORT_TERM, "Short-term tracker", "short-term"},
                                                {ZERO_TERM, "Zero-term tracker", "zero-term"},
                                                {0, NULL, NULL}};
#else

    static const GEnumValue tracking_types[] = {{SHORT_TERM, "Short-term tracker", "short-term"},
                                                {ZERO_TERM, "Zero-term tracker", "zero-term"},
                                                {0, NULL, NULL}};
#endif

    if (!gva_tracking_type)
        gva_tracking_type = g_enum_register_static("GstGvaTrackingType", tracking_types);

    return gva_tracking_type;
}
