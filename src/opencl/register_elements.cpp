/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/transform.h"

#ifdef ENABLE_VAAPI
#include "video_preproc_vaapi_opencl.h"
#endif
#include "tensor_normalize_opencl.h"

#include "gva_tensor_meta.h"

static gboolean plugin_init(GstPlugin *plugin) {
#ifdef ENABLE_VAAPI
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::VideoPreprocVAAPIOpenCLDesc))
        return FALSE;
#endif

    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::TensorNormalizeOpenCLDesc))
        return FALSE;

    // register metadata
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_opencl,
                  PRODUCT_FULL_NAME " elements based on OpenCL", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
