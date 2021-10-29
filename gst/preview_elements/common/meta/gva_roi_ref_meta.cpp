/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_roi_ref_meta.hpp"

GType gva_roi_ref_meta_api_get_type(void) {
    static volatile GType type;
    static const gchar *tags[] = {GVA_ROI_REF_META_TAG, NULL};

    if (g_once_init_enter(&type)) {
        GType _type = gst_meta_api_type_register(GVA_ROI_REF_META_API_NAME, tags);
        g_once_init_leave(&type, _type);
    }
    return type;
}

gboolean gva_roi_ref_meta_init(GstMeta *meta, gpointer /*params*/, GstBuffer * /*buffer*/) {
    GvaRoiRefMeta *roi_ref_meta = (GvaRoiRefMeta *)meta;
    roi_ref_meta->reference_roi_id = -1;

    return true;
}

void gva_roi_ref_meta_free(GstMeta * /*meta*/, GstBuffer * /*buffer*/) {
}

gboolean gva_roi_ref_meta_transform(GstBuffer * /*dest_buf*/, GstMeta * /*src_meta*/, GstBuffer * /*src_buf*/,
                                    GQuark /*type*/, gpointer /*data*/) {
    GST_ERROR("Transform is not implemented for GvaRoiRefMeta");
    return false;
}

const GstMetaInfo *gva_roi_ref_meta_get_info(void) {
    static const GstMetaInfo *meta_info = NULL;

    if (g_once_init_enter(&meta_info)) {
        const GstMetaInfo *meta =
            gst_meta_register(gva_roi_ref_meta_api_get_type(), GVA_ROI_REF_META_IMPL_NAME, sizeof(GvaRoiRefMeta),
                              (GstMetaInitFunction)gva_roi_ref_meta_init, gva_roi_ref_meta_free,
                              (GstMetaTransformFunction)gva_roi_ref_meta_transform);
        g_once_init_leave(&meta_info, meta);
    }
    return meta_info;
}
