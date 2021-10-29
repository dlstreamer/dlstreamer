/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file gva_roi_ref_meta.h
 * @brief This file contains helper functions to control GvaRoiRefMeta instances
 */

#pragma once

#include <gst/gst.h>

#define GVA_ROI_REF_META_API_NAME "GvaRoiRefMetaAPI"
#define GVA_ROI_REF_META_IMPL_NAME "GvaRoiRefMeta"
#define GVA_ROI_REF_META_TAG "gva_roi_ref_meta"

G_BEGIN_DECLS

typedef struct _GvaRoiRefMeta GvaRoiRefMeta;

struct _GvaRoiRefMeta {
    GstMeta meta;

    gint reference_roi_id;
};

const GstMetaInfo *gva_roi_ref_meta_get_info(void);

GType gva_roi_ref_meta_api_get_type(void);

#define GVA_ROI_REF_META_INFO (gva_roi_ref_meta_get_info())

#define GVA_ROI_REF_META_GET(buf) ((GvaRoiRefMeta *)gst_buffer_get_meta(buf, gva_roi_ref_meta_api_get_type()))

#define GVA_ROI_REF_META_ITERATE(buf, state)                                                                           \
    ((GvaRoiRefMeta *)gst_buffer_iterate_meta_filtered(buf, state, gva_roi_ref_meta_api_get_type()))

#define GVA_ROI_REF_META_ADD(buf) ((GvaRoiRefMeta *)gst_buffer_add_meta(buf, gva_roi_ref_meta_get_info(), NULL))

#define GVA_ROI_REF_META_COUNT(buf) (gst_buffer_get_n_meta(buf, gva_roi_ref_meta_api_get_type()))

G_END_DECLS
