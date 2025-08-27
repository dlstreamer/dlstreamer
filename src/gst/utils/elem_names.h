/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

namespace dlstreamer::elem {

// Deep Learning Streamer (DL Streamer) elements
constexpr const char *roi_split = "roi_split";
constexpr const char *rate_adjust = "rate_adjust";
constexpr const char *vaapi_batch_proc = "vaapi_batch_proc";
constexpr const char *vaapi_to_opencl = "vaapi_to_opencl";
constexpr const char *opencv_cropscale = "opencv_cropscale";
constexpr const char *tensor_convert = "tensor_convert";
constexpr const char *opencv_tensor_normalize = "opencv_tensor_normalize";
constexpr const char *opencl_tensor_normalize = "opencl_tensor_normalize";
constexpr const char *openvino_tensor_inference = "openvino_tensor_inference";
constexpr const char *openvino_video_inference = "openvino_video_inference";
constexpr const char *pytorch_tensor_inference = "pytorch_tensor_inference";
constexpr const char *tensor_postproc_ = "tensor_postproc_";
constexpr const char *batch_create = "batch_create";
constexpr const char *batch_split = "batch_split";
constexpr const char *meta_aggregate = "meta_aggregate";
constexpr const char *meta_smooth = "meta_smooth";
constexpr const char *opencv_meta_overlay = "opencv_meta_overlay";
constexpr const char *sycl_meta_overlay = "sycl_meta_overlay";
constexpr const char *capsrelax = "capsrelax";

// GStreamer elements
constexpr const char *queue = "queue";
constexpr const char *videoscale = "videoscale";
constexpr const char *videoconvert = "videoconvert";
constexpr const char *vaapipostproc = "vaapipostproc";
constexpr const char *vapostproc = "vapostproc";
constexpr const char *caps_system_memory = "capsfilter caps=video/x-raw";
constexpr const char *caps_vasurface_memory = "capsfilter caps=video/x-raw(memory:VASurface)";
constexpr const char *caps_vamemory_memory = "capsfilter caps=video/x-raw(memory:VAMemory)";

constexpr const char *pipe_separator = " ! ";

} // namespace dlstreamer::elem
