/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_WATERMARK_H_
#define _GST_GVA_WATERMARK_H_

#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_WATERMARK (gst_gva_watermark_get_type())
#define GST_GVA_WATERMARK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_WATERMARK, GstGvaWatermark))
#define GST_GVA_WATERMARK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_WATERMARK, GstGvaWatermarkClass))
#define GST_IS_GVA_WATERMARK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_WATERMARK))
#define GST_IS_GVA_WATERMARK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_WATERMARK))

typedef struct _GstGvaWatermark GstGvaWatermark;
typedef struct _GstGvaWatermarkClass GstGvaWatermarkClass;

enum WatermarkPath {
    WatermarkPathNone = 0,
    WatermarkPathDirect,
    WatermarkPathVaapi,
};

struct _GstGvaWatermark {
    GstBin base_gvawatermark;
    GstElement *identity;
    GstElement *watermarkimpl;
    GstPad *sinkpad;
    GstPad *srcpad;
    gchar *device;

    enum WatermarkPath active_path;
    GstElement *vaapi_path_elems[3];
    gulong block_probe_id;
};

struct _GstGvaWatermarkClass {
    GstBinClass base_gvawatermark_class;
};

GType gst_gva_watermark_get_type(void);

G_END_DECLS

#endif
