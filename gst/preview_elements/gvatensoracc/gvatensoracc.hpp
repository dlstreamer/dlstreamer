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

GST_DEBUG_CATEGORY_EXTERN(gva_tensor_acc_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_ACC gva_tensor_acc_debug_category

#define GST_TYPE_GVA_TENSOR_ACC (gva_tensor_acc_get_type())
#define GVA_TENSOR_ACC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_ACC, GvaTensorAcc))
#define GVA_TENSOR_ACC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_ACC, GvaTensorAccClass))
#define GST_IS_GVA_TENSOR_ACC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_ACC))
#define GST_IS_GVA_TENSOR_ACC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_ACC))

enum AccumulateMode { MODE_SLIDING_WINDOW, MODE_CONDITION };
enum AccumulateData { MEMORY, META };

typedef struct _GvaTensorAcc {
    GstBaseTransform base;

    struct _Props {
        /* properties */
        AccumulateMode mode;
        /* TODO: remove prefix 'window' because it is converter specific */
        guint window_step;
        guint window_size;
        AccumulateData data;

        std::unique_ptr<IAccumulator> accumulator;
    } props;
} GvaTensorAcc;

typedef struct _GvaTensorAccClass {
    GstBaseTransformClass base_class;
} GvaTensorAccClass;

GType gva_tensor_acc_get_type(void);

G_END_DECLS
