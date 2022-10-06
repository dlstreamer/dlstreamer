/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "processbin.h"
#include <gst/gst.h>
#include <list>
#include <map>
#include <string>

G_BEGIN_DECLS

#define VIDEO_INFERENCE_NAME "Generic inference element"
#define VIDEO_INFERENCE_DESCRIPTION "Runs Deep Learning inference on any model with RGB-like input"

GST_DEBUG_CATEGORY_EXTERN(video_inference_debug_category);

#define GST_TYPE_VIDEO_INFERENCE (video_inference_get_type())
#define VIDEO_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_VIDEO_INFERENCE, VideoInference))
#define VIDEO_INFERENCE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_VIDEO_INFERENCE, VideoInferenceClass))
#define VIDEO_INFERENCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_VIDEO_INFERENCE, VideoInferenceClass))

namespace elem {

// Intel® Deep Learning Streamer (Intel® DL Streamer) elements
constexpr const char *roi_split = "roi_split";
constexpr const char *rate_adjust = "rate_adjust";
constexpr const char *vaapi_batch_proc = "vaapi_batch_proc";
constexpr const char *vaapi_to_opencl = "vaapi_to_opencl";
constexpr const char *opencv_cropscale = "opencv_cropscale";
constexpr const char *tensor_convert = "tensor_convert";
constexpr const char *opencv_tensor_normalize = "opencv_tensor_normalize";
constexpr const char *opencl_tensor_normalize = "opencl_tensor_normalize";
constexpr const char *openvino_tensor_inference = "openvino_tensor_inference";
constexpr const char *tensor_postproc_ = "tensor_postproc_";
constexpr const char *batch_create = "batch_create";
constexpr const char *batch_split = "batch_split";
constexpr const char *meta_aggregate = "meta_aggregate";
constexpr const char *meta_repeat = "meta_repeat";

// GStreamer elements
constexpr const char *queue = "queue";
constexpr const char *videoscale = "videoscale";
constexpr const char *videoconvert = "videoconvert";
constexpr const char *vaapipostproc = "vaapipostproc";
constexpr const char *caps_system_memory = "capsfilter caps=video/x-raw";
constexpr const char *caps_vasurface_memory = "capsfilter caps=video/x-raw(memory:VASurface)";

} // namespace elem

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

    class VideoInferencePrivate *impl;

    void set_inference_element(const gchar *element);
    void set_postaggregate_element(const gchar *element);
} VideoInference;

typedef struct _VideoInferenceClass {
    GstProcessBinClass base;

    std::string (*get_default_postprocess_elements)(VideoInference *video_inference);
} VideoInferenceClass;

GType video_inference_get_type(void);

G_END_DECLS
