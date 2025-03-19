/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define META_SMOOTH_NAME "smooth metadata"
#define META_SMOOTH_DESCRIPTION META_SMOOTH_NAME

#define GST_TYPE_META_SMOOTH (meta_smooth_get_type())
#define META_SMOOTH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_META_SMOOTH, MetaSmooth))
#define META_SMOOTH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_META_SMOOTH, MetaSmoothClass))
#define GST_IS_META_SMOOTH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_META_SMOOTH))
#define GST_IS_META_SMOOTH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_META_SMOOTH))

typedef struct _MetaSmooth {
    GstBaseTransform base;
    struct MetaSmoothPrivate *impl;
} MetaSmooth;

typedef struct _MetaSmoothClass {
    GstBaseTransformClass base_class;
} MetaSmoothClass;

GType meta_smooth_get_type(void);

G_END_DECLS
