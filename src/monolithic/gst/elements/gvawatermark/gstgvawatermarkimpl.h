/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_WATERMARK_IMPL_H_
#define _GST_GVA_WATERMARK_IMPL_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_WATERMARK_IMPL (gst_gva_watermark_impl_get_type())
#define GST_GVA_WATERMARK_IMPL(obj)                                                                                    \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_WATERMARK_IMPL, GstGvaWatermarkImpl))
#define GST_GVA_WATERMARK_IMPL_CLASS(klass)                                                                            \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_WATERMARK_IMPL, GstGvaWatermarkImplClass))
#define GST_IS_GVA_WATERMARK_IMPL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_WATERMARK_IMPL))
#define GST_IS_GVA_WATERMARK_IMPL_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_WATERMARK_IMPL))

typedef struct _GstGvaWatermarkImpl GstGvaWatermarkImpl;
typedef struct _GstGvaWatermarkImplClass GstGvaWatermarkImplClass;

struct _GstGvaWatermarkImpl {
    GstBaseTransform base_transform;
    GstVideoInfo info;
    gchar *device;
    struct Impl *impl;
};

struct _GstGvaWatermarkImplClass {
    GstBaseTransformClass base_gvawatermark_class;
};

GType gst_gva_watermark_impl_get_type(void);

G_END_DECLS

#endif
