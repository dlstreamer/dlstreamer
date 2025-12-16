/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_ATTACH_ROI_H_
#define _GST_GVA_ATTACH_ROI_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <stdio.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_ATTACH_ROI (gst_gva_attach_roi_get_type())
#define GST_GVA_ATTACH_ROI(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_ATTACH_ROI, GstGvaAttachRoi))
#define GST_GVA_ATTACH_ROI_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_ATTACH_ROI, GstGvaAttachRoiClass))
#define GST_IS_GVA_ATTACH_ROI(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_ATTACH_ROI))
#define GST_IS_GVA_ATTACH_ROI_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_ATTACH_ROI))

typedef struct _GstGvaAttachRoi GstGvaAttachRoi;
typedef struct _GstGvaAttachRoiClass GstGvaAttachRoiClass;

#ifdef __cplusplus
struct GvaAttachRoiContext;
class AttachRoi;
#else
typedef struct _GvaAttachRoi GvaAttachRoiContext;
typedef struct _AttachRoi AttachRoi;
#endif

struct _GstGvaAttachRoi {
    GstBaseTransform base_gvaattachroi;
    gchar *filepath;
    gint mode;
    gchar *roi_prop;
    GstVideoInfo *info;

    AttachRoi *impl;
};

struct _GstGvaAttachRoiClass {
    GstBaseTransformClass base_gvaattachroi_class;
};

GType gst_gva_attach_roi_get_type(void);

G_END_DECLS

#endif
