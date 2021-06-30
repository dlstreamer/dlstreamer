/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "preprocessors/ipreproc.hpp"

#include <capabilities/types.hpp>
#include <common/input_model_preproc.h>
#include <inference_backend/pre_proc.h>

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <string>
#include <vector>

G_BEGIN_DECLS

#define GVA_VIDEO_TO_TENSOR_NAME "[Preview] Video To Tensor Converter Element"
#define GVA_VIDEO_TO_TENSOR_DESCRIPTION "Performs conversion of a video input data to tensor"

GST_DEBUG_CATEGORY_EXTERN(gst_gva_video_to_tensor_debug_category);
#define GST_DEBUG_CAT_GVA_VIDEO_TO_TENSOR gst_gva_video_to_tensor_debug_category

#define GST_TYPE_GVA_VIDEO_TO_TENSOR (gst_gva_video_to_tensor_get_type())
#define GST_GVA_VIDEO_TO_TENSOR(obj)                                                                                   \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_VIDEO_TO_TENSOR, GstGvaVideoToTensor))
#define GST_GVA_VIDEO_TO_TENSOR_CLASS(klass)                                                                           \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_VIDEO_TO_TENSOR, GstGvaVideoToTensorClass))
#define GST_IS_GVA_VIDEO_TO_TENSOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_VIDEO_TO_TENSOR))
#define GST_IS_GVA_VIDEO_TO_TENSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_VIDEO_TO_TENSOR))

enum PreProcBackend { OPENCV, IE, VAAPI_SURFACE_SHARING, VAAPI_SYSTEM };

typedef struct _GstGvaVideoToTensor {
    GstBaseTransform base;

    struct _Props {
        /* properties */
        std::string model_proc;
        PreProcBackend pre_proc_backend;

        /* tensor info */
        GstVideoInfo *input_info;
        TensorCaps tensor_caps;

        std::vector<ModelInputProcessorInfo::Ptr> input_processor_info;
        InferenceBackend::InputImageLayerDesc::Ptr pre_proc_info;
        std::unique_ptr<IPreProc> preprocessor;
    } props;

    void init_preprocessor();
    bool need_preprocessing() const;
    InferenceBackend::MemoryType get_output_mem_type(InferenceBackend::MemoryType input_mem_type) const;
} GstGvaVideoToTensor;

typedef struct _GstGvaVideoToTensorClass {
    GstBaseTransformClass base_class;
} GstGvaVideoToTensorClass;

GType gst_gva_video_to_tensor_get_type(void);

G_END_DECLS
