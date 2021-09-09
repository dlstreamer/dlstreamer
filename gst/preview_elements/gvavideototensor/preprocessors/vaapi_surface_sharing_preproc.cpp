/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_surface_sharing_preproc.hpp"

#ifdef ENABLE_VAAPI

#include <gva_custom_meta.hpp>

using namespace InferenceEngine;
using namespace InferenceBackend;

VaapiSurfaceSharingPreProc::VaapiSurfaceSharingPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info,
                                                       const TensorCaps &output_tensor_info)
    : _input_video_info(input_video_info), _output_tensor_info(output_tensor_info),
      _pre_proc_info(new InferenceEngine::PreProcessInfo()),
      _vaapi_pre_proc(
          new VaapiPreProc(display, input_video_info, output_tensor_info, FourCC::FOURCC_NV12, MemoryType::VAAPI)) {
    if (!_input_video_info)
        throw std::invalid_argument("GstVideoInfo is null");

    _pre_proc_info->setColorFormat(InferenceEngine::ColorFormat::NV12);
}

VaapiSurfaceSharingPreProc::~VaapiSurfaceSharingPreProc() = default;

void VaapiSurfaceSharingPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer) {
    _vaapi_pre_proc->process(in_buffer, out_buffer);

    GstGVACustomMeta *meta = GST_GVA_CUSTOM_META_ADD(out_buffer);
    g_assert(meta->pre_process_info == nullptr);
    // TODO: who should be responsible for cleanup
    meta->pre_process_info = _pre_proc_info.get();
    meta->width = GST_VIDEO_INFO_WIDTH(_input_video_info);
    meta->height = GST_VIDEO_INFO_HEIGHT(_input_video_info);
    meta->channels = 3;
}

void VaapiSurfaceSharingPreProc::process(GstBuffer *) {
    throw std::runtime_error("VaapiSurfaceSharingPreProc: In-place processing is not supported");
}

#endif
