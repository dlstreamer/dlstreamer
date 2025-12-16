/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_GVADESKEW_H__
#define __GST_GVADESKEW_H__

#include <gst/video/gstvideofilter.h>
#include <opencv2/opencv.hpp>

G_BEGIN_DECLS

#define GST_TYPE_GVADESKEW (gst_gvadeskew_get_type())
G_DECLARE_FINAL_TYPE(GstGvaDeskew, gst_gvadeskew, GST, GVADESKEW, GstVideoFilter)

// Explicit struct definition for G_DEFINE_TYPE
struct _GstGvaDeskew {
    GstVideoFilter parent_instance;
    gchar *intrinsics_file;
    cv::Mat K;
};

struct _GstGvaDeskewClass {
    GstVideoFilterClass parent_class;
};

G_END_DECLS

#endif /* __GST_GVADESKEW_H__ */