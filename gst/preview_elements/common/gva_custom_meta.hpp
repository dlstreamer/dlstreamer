/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/**
 * @file gva_custom_meta.h
 * @brief This file contains helper functions to control GstGVACustomMeta instances
 */

#pragma once

#include <gst/gst.h>

#include <ie_preprocess.hpp>

#define GVA_CUSTOM_META_API_NAME "GstGVACustomMetaAPI"
#define GVA_CUSTOM_META_IMPL_NAME "GstGVACustomMeta"
#define GVA_CUSTOM_META_TAG "gva_custom_meta"

G_BEGIN_DECLS

typedef struct _GstGVACustomMeta GstGVACustomMeta;

struct _GstGVACustomMeta {
    GstMeta meta;

    InferenceEngine::PreProcessInfo *pre_process_info;
    int channels;
    size_t width;
    size_t height;
};

const GstMetaInfo *gst_gva_custom_meta_get_info(void);

GType gst_gva_custom_meta_api_get_type(void);

#define GST_GVA_CUSTOM_META_INFO (gst_gva_custom_meta_get_info())

#define GST_GVA_CUSTOM_META_GET(buf) ((GstGVACustomMeta *)gst_buffer_get_meta(buf, gst_gva_custom_meta_api_get_type()))

#define GST_GVA_CUSTOM_META_ITERATE(buf, state)                                                                        \
    ((GstGVACustomMeta *)gst_buffer_iterate_meta_filtered(buf, state, gst_gva_custom_meta_api_get_type()))

#define GST_GVA_CUSTOM_META_ADD(buf)                                                                                   \
    ((GstGVACustomMeta *)gst_buffer_add_meta(buf, gst_gva_custom_meta_get_info(), NULL))

#define GST_GVA_CUSTOM_META_COUNT(buf) (gst_buffer_get_n_meta(buf, gst_gva_custom_meta_api_get_type()))

G_END_DECLS
