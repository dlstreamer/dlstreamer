/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_DEBUG_CAT_GVA_DROP gva_drop_debug_category

#define GST_TYPE_GVA_DROP (gva_drop_get_type())
#define GVA_DROP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_DROP, GvaDrop))
#define GVA_DROP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_DROP, GvaDropClass))
#define GST_IS_GVA_DROP(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_DROP))
#define GST_IS_GVA_DROP_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_DROP))
typedef struct _GvaDrop GvaDrop;
typedef struct _GvaDropClass GvaDropClass;

enum DropMode { DEFAULT, GAP_EVENT };

struct _GvaDrop {
    GstBaseTransform parent;
    /* public properties */
    guint pass_frames;
    guint drop_frames;
    DropMode mode;

    /* private properties */
    guint frames_counter;
};

struct _GvaDropClass {
    GstBaseTransformClass parent_class;
};

GType gva_drop_get_type(void);

G_END_DECLS
