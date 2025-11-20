/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_types.h"

#define UNKNOWN_TYPE_NAME "unknown"
#define ZERO_TERM_NAME "zero-term"
#define SHORT_TERM_IMAGELESS_NAME "short-term-imageless"
#define ZERO_TERM_IMAGELESS_NAME "zero-term-imageless"
#define DEEP_SORT_NAME "deep-sort"

GType gst_gva_get_tracking_type(void) {
    static GType gva_tracking_type = 0;
    static const GEnumValue tracking_types[] = {
        {ZERO_TERM, "Zero-term tracker", ZERO_TERM_NAME},
        {SHORT_TERM_IMAGELESS, "Short-term imageless tracker", SHORT_TERM_IMAGELESS_NAME},
        {ZERO_TERM_IMAGELESS, "Zero-term imageless tracker", ZERO_TERM_IMAGELESS_NAME},
        {DEEP_SORT, "Deep SORT tracker with visual features", DEEP_SORT_NAME},
        {0, NULL, NULL}};

    if (!gva_tracking_type)
        gva_tracking_type = g_enum_register_static("GstGvaTrackingType", tracking_types);

    return gva_tracking_type;
}

const gchar *tracking_type_to_string(GstGvaTrackingType type) {
    switch (type) {
    case ZERO_TERM:
        return ZERO_TERM_NAME;
    case SHORT_TERM_IMAGELESS:
        return SHORT_TERM_IMAGELESS_NAME;
    case ZERO_TERM_IMAGELESS:
        return ZERO_TERM_IMAGELESS_NAME;
    case DEEP_SORT:
        return DEEP_SORT_NAME;
    default:
        return UNKNOWN_TYPE_NAME;
    }
}
