/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "converters/iaccumulator.hpp"

#include <gst/base/gstbasetransform.h>
#include <gst/base/gstqueuearray.h>
#include <gst/gst.h>

#include <memory>

G_BEGIN_DECLS

#define GVA_TENSOR_ACC_NAME "[Preview] Generic Accumulate Element"
#define GVA_TENSOR_ACC_DESCRIPTION "Performs accumulation of an input data"

GST_DEBUG_CATEGORY_EXTERN(gst_gva_tensor_acc_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_ACC gst_gva_tensor_acc_debug_category

#define GST_TYPE_GVA_TENSOR_ACC (gst_gva_tensor_acc_get_type())
#define GST_GVA_TENSOR_ACC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_ACC, GstGvaTensorAcc))
#define GST_GVA_TENSOR_ACC_CLASS(klass)                                                                                \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_ACC, GstGvaTensorAccClass))
#define GST_IS_GVA_TENSOR_ACC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_ACC))
#define GST_IS_GVA_TENSOR_ACC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_ACC))

enum AccumulateMode { MODE_SLIDING_WINDOW };

typedef struct _GstGvaTensorAcc {
    GstBaseTransform base;

    struct _Props {
        AccumulateMode mode;

        /* TODO: remove prefix 'window' because it is converter specific */
        guint window_step;
        guint window_size;

        std::unique_ptr<IAccumulator> accumulator;
    } props;
} GstGvaTensorAcc;

typedef struct _GstGvaTensorAccClass {
    GstBaseTransformClass base_class;
} GstGvaTensorAccClass;

GType gst_gva_tensor_acc_get_type(void);

G_END_DECLS
