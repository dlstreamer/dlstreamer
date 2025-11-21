/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

G_BEGIN_DECLS

/* Forward declarations; struct is defined privately in the .cpp to allow
 * internal fields without exposing them as public API. */
typedef struct _GstGvaMotionDetect GstGvaMotionDetect;
typedef struct _GstGvaMotionDetectClass GstGvaMotionDetectClass;

#define GST_TYPE_GVA_MOTION_DETECT (gst_gva_motion_detect_get_type())
GType gst_gva_motion_detect_get_type(void);

/* Cast and type check macros (manual since we use forward declarations) */
#define GST_GVA_MOTION_DETECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_MOTION_DETECT, GstGvaMotionDetect))
#define GST_GVA_MOTION_DETECT_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_MOTION_DETECT, GstGvaMotionDetectClass))
#define GST_IS_GVA_MOTION_DETECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_MOTION_DETECT))
#define GST_IS_GVA_MOTION_DETECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_MOTION_DETECT))

G_END_DECLS
