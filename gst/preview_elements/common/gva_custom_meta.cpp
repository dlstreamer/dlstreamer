/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_custom_meta.hpp"

GType gst_gva_custom_meta_api_get_type(void) {
    static volatile GType type;
    static const gchar *tags[] = {GVA_CUSTOM_META_TAG, NULL};

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register(GVA_CUSTOM_META_API_NAME, tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

gboolean gst_gva_custom_meta_init(GstMeta *meta, gpointer /*params*/, GstBuffer * /*buffer*/) {
    GstGVACustomMeta *custom_meta = (GstGVACustomMeta *)meta;
    custom_meta->pre_process_info = nullptr;

    return TRUE;
}

void gst_gva_custom_meta_free(GstMeta *meta, GstBuffer * /*buffer*/) {
    GstGVACustomMeta *custom_meta = (GstGVACustomMeta *)meta;
    custom_meta->pre_process_info = nullptr;
}

gboolean gst_gva_custom_meta_transform(GstBuffer * /*dest_buf*/, GstMeta * /*src_meta*/, GstBuffer * /*src_buf*/,
                                       GQuark /*type*/, gpointer /*data*/) {
    GST_ERROR("Transform is not implemented for GvaCustomMeta");
    return false;
}

const GstMetaInfo *gst_gva_custom_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta =
            gst_meta_register(gst_gva_custom_meta_api_get_type(), GVA_CUSTOM_META_IMPL_NAME, sizeof(GstGVACustomMeta),
                              (GstMetaInitFunction)gst_gva_custom_meta_init, gst_gva_custom_meta_free,
                              (GstMetaTransformFunction)gst_gva_custom_meta_transform);
        g_once_init_leave(&meta_info, meta);
    }
    return meta_info;
}
