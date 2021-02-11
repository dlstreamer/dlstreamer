/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_SEGMENT_H_
#define _GST_GVA_SEGMENT_H_

#include "gva_base_inference.h"

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_SEGMENT (gst_gva_segment_get_type())
#define GST_GVA_SEGMENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_SEGMENT, GstGvaSegment))
#define GST_GVA_SEGMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_SEGMENT, GstGvaSegmentClass))
#define GST_IS_GVA_SEGMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_SEGMENT))
#define GST_IS_GVA_SEGMENT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_SEGMENT))

typedef struct _GstGvaSegment {
    GvaBaseInference base_inference;
} GstGvaSegment;

typedef struct _GstGvaSegmentClass {
    GvaBaseInferenceClass base_class;
} GstGvaSegmentClass;

GType gst_gva_segment_get_type(void);

G_END_DECLS

#endif