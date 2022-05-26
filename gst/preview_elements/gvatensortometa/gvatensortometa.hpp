/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include <capabilities/types.hpp>
#include <tensor_layer_desc.hpp>

#include "post_processor.h"
#include "post_processor/post_proc_common.h"

G_BEGIN_DECLS

#define GVA_TENSOR_TO_META_NAME "[Preview] Tensor To Meta Converter Element"
#define GVA_TENSOR_TO_META_DESCRIPTION "Performs conversion of a tensor input data to meta"

GST_DEBUG_CATEGORY_EXTERN(gva_tensor_to_meta_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_TO_META gva_tensor_to_meta_debug_category

#define GST_TYPE_GVA_TENSOR_TO_META (gva_tensor_to_meta_get_type())
#define GVA_TENSOR_TO_META(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_TO_META, GvaTensorToMeta))
#define GVA_TENSOR_TO_META_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_TO_META, GvaTensorToMetaClass))
#define GST_IS_GVA_TENSOR_TO_META(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_TO_META))
#define GST_IS_GVA_TENSOR_TO_META_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_TO_META))

typedef struct _GvaTensorToMeta {
    GstBaseTransform base;

    class GvaTensorToMetaPrivate *impl;
} GvaTensorToMeta;

typedef struct _GvaTensorToMetaClass {
    GstBaseTransformClass base_class;
} GvaTensorToMetaClass;

GType gva_tensor_to_meta_get_type(void);

#define GST_TYPE_GVA_TENSOR_TO_META_CONVERTER_TYPE (gva_tensor_to_meta_converter_get_type())
GType gva_tensor_to_meta_converter_get_type(void);

G_END_DECLS
