/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "mtcnn_common.h"

#define UNKNOWN_VALUE_NAME "unknown"
#define MODE_PNET_NAME "pnet"
#define MODE_RNET_NAME "rnet"
#define MODE_ONET_NAME "onet"

gboolean foreach_meta_remove_one(GstBuffer *buffer, GstMeta **meta, gpointer to_remove) {
    if (!meta || !(*meta))
        return FALSE;

    if ((*meta)->info->api == GPOINTER_TO_SIZE(to_remove)) {
        GST_META_FLAG_UNSET(*meta, GST_META_FLAG_LOCKED);
        return gst_buffer_remove_meta(buffer, *meta);
    }
    return TRUE;
}

GType gst_mtcnn_get_mode_type(void) {
    static GType mtcnn_mode_type = 0;
    static const GEnumValue mode_types[] = {{MODE_PNET, "P-network mode", MODE_PNET_NAME},
                                            {MODE_RNET, "R-network mode", MODE_RNET_NAME},
                                            {MODE_ONET, "O-network mode", MODE_ONET_NAME},
                                            {0, NULL, NULL}};
    if (!mtcnn_mode_type)
        mtcnn_mode_type = g_enum_register_static("GstMTCNNModeType", mode_types);
    return mtcnn_mode_type;
}

const gchar *mode_type_to_string(GstMTCNNModeType mode) {
    switch (mode) {
    case MODE_PNET:
        return MODE_PNET_NAME;
    case MODE_RNET:
        return MODE_RNET_NAME;
    case MODE_ONET:
        return MODE_ONET_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}
