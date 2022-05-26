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

#include "inference_storage.hpp"
#include "tensor_inference.hpp"

G_BEGIN_DECLS

#define GVA_TENSOR_INFERENCE_NAME "[Preview] Generic Inference Element"
#define GVA_TENSOR_INFERENCE_DESCRIPTION "Performs inference on an input data"

GST_DEBUG_CATEGORY_EXTERN(gva_tensor_inference_debug_category);
#define GST_DEBUG_CAT_GVA_TENSOR_INFERENCE gva_tensor_inference_debug_category

#define GST_TYPE_GVA_TENSOR_INFERENCE (gva_tensor_inference_get_type())
#define GVA_TENSOR_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TENSOR_INFERENCE, GvaTensorInference))
#define GVA_TENSOR_INFERENCE_CLASS(klass)                                                                              \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TENSOR_INFERENCE, GvaTensorInferenceClass))
#define GST_IS_GVA_TENSOR_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TENSOR_INFERENCE))
#define GST_IS_GVA_TENSOR_INFERENCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TENSOR_INFERENCE))

typedef struct _GvaTensorInference {
    GstBaseTransform base;

    // TODO: think about making it simplier
    // Complex C++ types need to be created with ctors/dtors
    struct _Props {
        /* properties */
        std::string model;
        std::string ie_config;
        std::string device;
        std::string instance_id;
        guint nireq;
        guint batch_size;

        TensorCapsArray input_caps;
        TensorCapsArray output_caps;

        GstVideoInfo *input_video_info;

        std::shared_ptr<TensorInference> infer;
        InferenceQueue<GstBuffer *> infer_queue;
        std::shared_ptr<MemoryPool> infer_pool;
    } props;

    void RunInference(GstBuffer *inbuf, GstBuffer *outbuf);
} GvaTensorInference;

typedef struct _GvaTensorInferenceClass {
    GstBaseTransformClass base_class;
} GvaTensorInferenceClass;

GType gva_tensor_inference_get_type(void);

G_END_DECLS
