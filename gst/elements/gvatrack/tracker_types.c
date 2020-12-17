/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_types.h"

GType gst_gva_get_tracking_type(void) {
    static GType gva_tracking_type = 0;
    static const GEnumValue tracking_types[] = {
#ifdef DOWNLOAD_VAS_TRACKER
        {SHORT_TERM, "Short-term tracker", "short-term"},
        {ZERO_TERM, "Zero-term tracker", "zero-term"},
#endif
        {SHORT_TERM_IMAGELESS, "Short-term imageless tracker", "short-term-imageless"},
        {ZERO_TERM_IMAGELESS, "Zero-term imageless tracker", "zero-term-imageless"},
        {0, NULL, NULL}};

    if (!gva_tracking_type)
        gva_tracking_type = g_enum_register_static("GstGvaTrackingType", tracking_types);

    return gva_tracking_type;
}
