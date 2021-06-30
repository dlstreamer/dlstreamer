/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "post_processor.hpp"
#include <capabilities/types.hpp>

G_BEGIN_DECLS

#define GVA_TENSOR_TO_META_NAME "[Preview] Tensor To Meta Converter Element"
#define GVA_TENSOR_TO_META_DESCRIPTION "Performs conversion of a tensor input data to meta"

GST_DEBUG_CATEGORY_EXTERN(gst_gva_tensor_to_meta_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_TO_META gst_gva_tensor_to_meta_debug_category

#define GST_TYPE_GVA_TENSOR_TO_META (gst_gva_tensor_to_meta_get_type())
#define GST_GVA_TENSOR_TO_META(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_TO_META, GstGvaTensorToMeta))
#define GST_GVA_TENSOR_TO_META_CLASS(klass)                                                                            \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_TO_META, GstGvaTensorToMetaClass))
#define GST_IS_GVA_TENSOR_TO_META(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_TO_META))
#define GST_IS_GVA_TENSOR_TO_META_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_TO_META))

typedef struct _GstGvaTensorToMeta {
    GstBaseTransform base;

    struct _Props {
        /* properties */
        std::string model_proc;

        /* internal */
        TensorCaps tensor_caps;
        std::unique_ptr<PostProcessor> postproc;
    } props;
} GstGvaTensorToMeta;

typedef struct _GstGvaTensorToMetaClass {
    GstBaseTransformClass base_class;
} GstGvaTensorToMetaClass;

GType gst_gva_tensor_to_meta_get_type(void);

G_END_DECLS
