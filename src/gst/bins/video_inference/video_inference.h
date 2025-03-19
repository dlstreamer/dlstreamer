/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "processbin.h"
#include <gst/gst.h>
#include <list>
#include <map>
#include <memory>
#include <string>

G_BEGIN_DECLS

#define VIDEO_INFERENCE_NAME "Generic inference element"
#define VIDEO_INFERENCE_DESCRIPTION "Runs Deep Learning inference on any model with RGB-like input"

#define GST_TYPE_VIDEO_INFERENCE (video_inference_get_type())
#define VIDEO_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VIDEO_INFERENCE, VideoInference))
#define VIDEO_INFERENCE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VIDEO_INFERENCE, VideoInferenceClass))
#define VIDEO_INFERENCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VIDEO_INFERENCE, VideoInferenceClass))

enum class PreProcessBackend : int {
    AUTO = 0,
    GST_OPENCV = 1,
    VAAPI = 2,
    VAAPI_TENSORS = 3,
    VAAPI_SURFACE_SHARING = 4,
    VAAPI_OPENCL = 5
};

// GType for property 'pre-process-backend'
GType preprocess_backend_get_type(void);

enum class Region { FULL_FRAME, ROI_LIST };

// GType for property 'inference-region'
GType inference_region_get_type();

typedef struct _VideoInference {
    GstProcessBin base;

    std::shared_ptr<class VideoInferencePrivate> impl;

    const gchar *get_model();
    void set_inference_element(const gchar *element);
    void set_postaggregate_element(const gchar *element);
} VideoInference;

typedef struct _VideoInferenceClass {
    GstProcessBinClass base;

    std::string (*get_default_postprocess_elements)(VideoInference *video_inference);
} VideoInferenceClass;

GType video_inference_get_type(void);

G_END_DECLS
