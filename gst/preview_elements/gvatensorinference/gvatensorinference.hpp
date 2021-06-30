/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include <memory>
#include <string>
#include <tuple>

#include <capabilities/types.hpp>
#include <memory_type.hpp>

#include "tensor_inference.hpp"

G_BEGIN_DECLS

#define GVA_TENSOR_INFERENCE_NAME "[Preview] Generic Inference Element"
#define GVA_TENSOR_INFERENCE_DESCRIPTION "Performs inference on an input data"

GST_DEBUG_CATEGORY_EXTERN(gst_gva_tensor_inference_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_INFERENCE gst_gva_tensor_inference_debug_category

#define GST_TYPE_GVA_TENSOR_INFERENCE (gst_gva_tensor_inference_get_type())
#define GST_GVA_TENSOR_INFERENCE(obj)                                                                                  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_INFERENCE, GstGvaTensorInference))
#define GST_GVA_TENSOR_INFERENCE_CLASS(klass)                                                                          \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_INFERENCE, GstGvaTensorInferenceClass))
#define GST_IS_GVA_TENSOR_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_INFERENCE))
#define GST_IS_GVA_TENSOR_INFERENCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_INFERENCE))

typedef struct _GstGvaTensorInference {
    GstBaseTransform base;

    // TODO: think about making it simplier
    // Complex C++ types need to be created with ctors/dtors
    struct _Props {
        /* properties */
        std::string model;
        std::string ie_config;
        std::string device;
        guint nireq;
        guint batch_size;

        TensorCaps input_caps;
        TensorCaps output_caps;

        std::unique_ptr<TensorInference> infer;
    } props;

    void RunInference(GstBuffer *inbuf, GstBuffer *outbuf);
} GstGvaTensorInference;

typedef struct _GstGvaTensorInferenceClass {
    GstBaseTransformClass base_class;
} GstGvaTensorInferenceClass;

GType gst_gva_tensor_inference_get_type(void);
gboolean gva_tensor_inference_stopped(GstGvaTensorInference *self);

G_END_DECLS
