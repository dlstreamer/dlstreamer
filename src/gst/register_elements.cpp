/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "dlstreamer/gst/transform.h"
#include "gva_tensor_meta.h"
#include <gst/gst.h>

#include "meta_aggregate.h"
#include "rate_adjust.h"
#include "tensor_convert.h"
#include "tensor_split_batch.h"
#include "video_roi_split.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "meta_aggregate", GST_RANK_NONE, GST_TYPE_META_AGGREGATE))
        return FALSE;
    if (!gst_element_register(plugin, "video_roi_split", GST_RANK_NONE, GST_TYPE_ROI_SPLIT))
        return FALSE;
    if (!gst_element_register(plugin, "tensor_split_batch", GST_RANK_NONE, GST_TYPE_TENSOR_SPLIT_BATCH))
        return FALSE;

    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::TensorConvertDesc))
        return FALSE;
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::RateAdjustDesc))
        return FALSE;

    // register metadata
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_elements,
                  PRODUCT_FULL_NAME " elements implemented directly on GStreamer API", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
