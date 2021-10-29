/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_surface_sharing_preproc.hpp"

#ifdef ENABLE_VAAPI

using namespace InferenceEngine;
using namespace InferenceBackend;

VaapiSurfaceSharingPreProc::VaapiSurfaceSharingPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info,
                                                       const TensorCaps &output_tensor_info,
                                                       const InputImageLayerDesc::Ptr &input_pre_proc_info)
    : _input_video_info(gst_video_info_copy(input_video_info)), _output_tensor_info(output_tensor_info),
      _pre_proc_info(new InferenceEngine::PreProcessInfo()),
      _vaapi_pre_proc(new VaapiPreProc(display, input_video_info, output_tensor_info, input_pre_proc_info,
                                       FourCC::FOURCC_NV12, MemoryType::VAAPI)) {
    if (!_input_video_info)
        throw std::invalid_argument("VaapiSurfaceSharingPreProc: GstVideoInfo is null");
    gst_video_info_set_format(_input_video_info, GST_VIDEO_FORMAT_NV12, output_tensor_info.GetWidth(),
                              output_tensor_info.GetHeight());

    _pre_proc_info->setColorFormat(InferenceEngine::ColorFormat::NV12);
}

VaapiSurfaceSharingPreProc::~VaapiSurfaceSharingPreProc() {
    gst_video_info_free(_input_video_info);
}

void VaapiSurfaceSharingPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer) {
    if (!in_buffer || !out_buffer)
        throw std::invalid_argument("VaapiSurfaceSharingPreProc: GstBuffer is null");

    _vaapi_pre_proc->process(in_buffer, out_buffer);
}

void VaapiSurfaceSharingPreProc::process(GstBuffer *) {
    throw std::runtime_error("VaapiSurfaceSharingPreProc: In-place processing is not supported");
}

void VaapiSurfaceSharingPreProc::flush() {
    _vaapi_pre_proc->flush();
}

#endif
