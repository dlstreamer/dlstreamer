/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_GVA_NMS_H__
#define __GST_GVA_NMS_H__

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include "mtcnn_common.h"

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_GVA_NMS (gst_gva_nms_get_type())
#define GST_GVA_NMS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_NMS, GstGvaNms))
#define GST_GVA_NMS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_NMS, GstGvaNmsClass))
#define GST_IS_GVA_NMS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_NMS))
#define GST_IS_GVA_NMS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_NMS))

typedef struct _GstGvaNms {
    GstBaseTransform base_gvanms;

    GstMTCNNModeType mode;
    gboolean merge;
    guint threshold;
    GstVideoInfo *info;
} GstGvaNms;

typedef struct _GstGvaNmsClass {
    GstBaseTransformClass base_gvanms_class;
} GstGvaNmsClass;

GType gst_gva_nms_get_type(void);

G_END_DECLS

#endif /* __GST_GVA_NMS_H__ */
