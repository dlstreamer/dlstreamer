/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define TENSOR_SPLIT_BATCH_NAME "[Preview] ROI Split Element"
#define TENSOR_SPLIT_BATCH_DESCRIPTION "Performs splitting of ROIs on incoming buffer"

GST_DEBUG_CATEGORY_EXTERN(tensor_split_batch_debug_category);
#define GST_DEBUG_CAT_TENSOR_SPLIT_BATCH tensor_split_batch_debug_category

#define GST_TYPE_TENSOR_SPLIT_BATCH (tensor_split_batch_get_type())
#define TENSOR_SPLIT_BATCH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TENSOR_SPLIT_BATCH, TensorSplitBatch))
#define TENSOR_SPLIT_BATCH_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TENSOR_SPLIT_BATCH, TensorSplitBatchClass))
#define GST_IS_TENSOR_SPLIT_BATCH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TENSOR_SPLIT_BATCH))
#define GST_IS_TENSOR_SPLIT_BATCH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TENSOR_SPLIT_BATCH))

typedef struct _TensorSplitBatch {
    GstBaseTransform base;
} TensorSplitBatch;

typedef struct _TensorSplitBatchClass {
    GstBaseTransformClass base_class;
} TensorSplitBatchClass;

GST_EXPORT GType tensor_split_batch_get_type(void);

G_END_DECLS
