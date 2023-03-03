/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_CAPS_RELAX (gst_capsrelax_get_type())

typedef struct _GstCapsRelax GstCapsRelax;
typedef struct _GstCapsRelaxClass GstCapsRelaxClass;

struct _GstCapsRelax {
    GstBaseTransform parent;
};

struct _GstCapsRelaxClass {
    GstBaseTransformClass parent_class;
};

G_GNUC_INTERNAL GType gst_capsrelax_get_type();

G_END_DECLS