/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef _GST_GVA_IDENTIFY_H_
#define _GST_GVA_IDENTIFY_H_

#include <gst/base/gstbasetransform.h>

#include "identify.h"

G_BEGIN_DECLS

#define GST_TYPE_GVA_IDENTIFY (gst_gva_identify_get_type())
#define GST_GVA_IDENTIFY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_IDENTIFY, GstGvaIdentify))
#define GST_GVA_IDENTIFY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_IDENTIFY, GstGvaIdentifyClass))
#define GST_IS_GVA_IDENTIFY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_IDENTIFY))
#define GST_IS_GVA_IDENTIFY_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_IDENTIFY))

typedef struct _GstGvaIdentify GstGvaIdentify;
typedef struct _GstGvaIdentifyClass GstGvaIdentifyClass;

struct _GstGvaIdentify {
    GstBaseTransform base_gvaidentify;

    gchar *model;
    gchar *gallery;
    gboolean initialized;
    gdouble threshold;
    GstVideoInfo *info;

    Identify *identifier;
};

struct _GstGvaIdentifyClass {
    GstBaseTransformClass base_gvaidentify_class;
};

GType gst_gva_identify_get_type(void);

G_END_DECLS

#endif
