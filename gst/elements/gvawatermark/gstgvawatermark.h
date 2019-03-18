/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_WATERMARK_H_
#define _GST_GVA_WATERMARK_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include "fps_meter.h"

G_BEGIN_DECLS

#define GST_TYPE_GVA_WATERMARK (gst_gva_watermark_get_type())
#define GST_GVA_WATERMARK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_WATERMARK, GstGvaWatermark))
#define GST_GVA_WATERMARK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_WATERMARK, GstGvaWatermarkClass))
#define GST_IS_GVA_WATERMARK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_WATERMARK))
#define GST_IS_GVA_WATERMARK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_WATERMARK))

typedef struct _GstGvaWatermark GstGvaWatermark;
typedef struct _GstGvaWatermarkClass GstGvaWatermarkClass;

struct _GstGvaWatermark {
    GstBaseTransform base_gvawatermark;
    fps_meter_s fps_meter;
    GstVideoInfo info;
};

struct _GstGvaWatermarkClass {
    GstBaseTransformClass base_gvawatermark_class;
};

GType gst_gva_watermark_get_type(void);

G_END_DECLS

#endif
