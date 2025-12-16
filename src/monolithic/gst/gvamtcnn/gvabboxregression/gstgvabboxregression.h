/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_GVA_BBOX_REGRESSION_H__
#define __GST_GVA_BBOX_REGRESSION_H__

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

#include "mtcnn_common.h"

G_BEGIN_DECLS

#define GST_TYPE_GVA_BBOX_REGRESSION (gst_gva_bbox_regression_get_type())
#define GST_GVA_BBOX_REGRESSION(obj)                                                                                   \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_BBOX_REGRESSION, GstGvaBBoxRegression))
#define GST_GVA_BBOX_REGRESSION_CLASS(klass)                                                                           \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_BBOX_REGRESSION, GstGvaBBoxRegressionClass))
#define GST_IS_GVA_BBOX_REGRESSION(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_BBOX_REGRESSION))
#define GST_IS_GVA_BBOX_RERESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_BBOX_REGRESSION))

typedef struct _GstGvaBBoxRegression {
    GstBaseTransform base_bboxregression;

    GstMTCNNModeType mode;
    GstVideoInfo *info;
} GstGvaBBoxRegression;

typedef struct _GstGvaBBoxRegressionClass {
    GstBaseTransformClass base_bboxregression_class;
} GstGvaBBoxRegressionClass;

GType gst_gva_bbox_regression_get_type(void);

G_END_DECLS

#endif /* __GST_GVA_BBOX_REGRESSION_H__ */
