/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_TENSOR_CONVERTER (gva_tensor_conv_get_type())
#define GVA_TENSOR_CONVERTER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_CONVERTER, GvaTensorConverter))

struct GvaTensorConverter {
    GstBaseTransform base;
    class GvaTensorConverterPrivate *impl;
};

struct GvaTensorConverterClass {
    GstBaseTransformClass base_class;
};

GType gva_tensor_conv_get_type();

G_END_DECLS
