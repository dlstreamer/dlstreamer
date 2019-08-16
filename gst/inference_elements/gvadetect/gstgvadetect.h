/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_DETECT_H_
#define _GST_GVA_DETECT_H_

#include <gst/base/gstbasetransform.h>

#include "gva_base_inference.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

G_BEGIN_DECLS

#define GST_TYPE_GVA_DETECT (gst_gva_detect_get_type())
#define GST_GVA_DETECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_DETECT, GstGvaDetect))
#define GST_GVA_DETECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_DETECT, GstGvaDetectClass))
#define GST_IS_GVA_DETECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_DETECT))
#define GST_IS_GVA_DETECT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_DETECT))

typedef struct _GstGvaDetect {
    GvaBaseInference base_inference;
    double threshold;
} GstGvaDetect;

typedef struct _GstGvaDetectClass {
    GvaBaseInferenceClass base_class;
} GstGvaDetectClass;

GType gst_gva_detect_get_type(void);

G_END_DECLS

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif
