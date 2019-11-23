/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_SPEEDOMETER_H_
#define _GST_GVA_SPEEDOMETER_H_

#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_SPEEDOMETER (gst_gva_speedometer_get_type())
#define GST_GVA_SPEEDOMETER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_SPEEDOMETER, GstGvaSpeedometer))
#define GST_GVA_SPEEDOMETER_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_SPEEDOMETER, GstGvaSpeedometerClass))
#define GST_IS_GVA_SPEEDOMETER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_SPEEDOMETER))
#define GST_IS_GVA_SPEEDOMETER_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_SPEEDOMETER))

typedef struct _GstGvaSpeedometer GstGvaSpeedometer;
typedef struct _GstGvaSpeedometerClass GstGvaSpeedometerClass;

struct _GstGvaSpeedometer {
    GstBaseTransform base_gvaSpeedometer;
    gchar *interval;
    guint skip_frames;
};

struct _GstGvaSpeedometerClass {
    GstBaseTransformClass base_gvaSpeedometer_class;
};

GType gst_gva_speedometer_get_type(void);

G_END_DECLS

#endif
