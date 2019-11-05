/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_POSEESTIMATE_H_
#define _GST_GVA_POSEESTIMATE_H_

#include <gst/base/gstbasetransform.h>

#include "gva_base_inference.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

G_BEGIN_DECLS

#define GST_TYPE_GVA_POSEESTIMATE (gst_gva_detect_get_type())
#define GST_GVA_POSEESTIMATE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_POSEESTIMATE, GstGvaPoseestimate))
#define GST_GVA_POSEESTIMATE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_POSEESTIMATE, GstGvaPoseestimateClass))
#define GST_IS_GVA_POSEESTIMATE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_POSEESTIMATE))
#define GST_IS_GVA_POSEESTIMATE_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_POSEESTIMATE))

typedef struct _GstGvaPoseestimate {
    GvaBaseInference base_inference;
} GstGvaPoseestimate;

typedef struct _GstGvaPoseestimateClass {
    GvaBaseInferenceClass base_class;
} GstGvaPoseestimateClass;

GType gst_gva_poseestimate_get_type(void);

G_END_DECLS

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif
