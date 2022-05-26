/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_preproc.hpp"

#ifdef ENABLE_VAAPI

#include <frame_data.hpp>
#include <opencv_utils/opencv_utils.h>
#include <utils.h>
#include <vaapi_converter.h>
#include <vaapi_image_info.hpp>
#include <vaapi_images.h>

using namespace InferenceBackend;

namespace {

constexpr size_t _VA_IMAGE_POOL_SIZE = 5;

} // namespace

VaapiPreProc::VaapiPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info,
                           const TensorCaps &output_tensor_info, const InputImageLayerDesc::Ptr &pre_proc_info,
                           InferenceBackend::FourCC format, MemoryType out_memory_type)
    : _input_video_info(input_video_info), _output_tensor_info(output_tensor_info), _pre_proc_info(pre_proc_info),
      _out_memory_type(out_memory_type), _va_context(new VaApiContext(display)),
      _va_converter(new VaApiConverter(_va_context.get())) {
    if (!input_video_info)
        throw std::invalid_argument("VaapiPreProc: GstVideoInfo is null");

    uint32_t batch = _output_tensor_info.HasBatchSize() ? _output_tensor_info.GetBatchSize() : 1;

    _va_image_pool.reset(
        new VaApiImagePool(_va_context.get(), VaApiImagePool::SizeParams(_VA_IMAGE_POOL_SIZE),
                           VaApiImagePool::ImageInfo{static_cast<uint32_t>(output_tensor_info.GetWidth()),
                                                     static_cast<uint32_t>(output_tensor_info.GetHeight()), batch,
                                                     format, _out_memory_type}));
}

VaapiPreProc::~VaapiPreProc() = default;

void *VaapiPreProc::displayRaw() const {
    assert(_va_context);
    return _va_context->DisplayRaw();
}

void VaapiPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer, GstVideoRegionOfInterestMeta *roi) {
    if (!in_buffer || !out_buffer)
        throw std::invalid_argument("VaapiPreProc: GstBuffer is null");

    FrameData src;
    src.Map(in_buffer, _input_video_info, InferenceBackend::MemoryType::VAAPI, GST_MAP_READ);

    FrameData dst;
    // TODO: always 3-channel output for vaapi preproc
    dst.Map(out_buffer, _output_tensor_info, GST_MAP_WRITE, InferenceBackend::MemoryType::SYSTEM, 3);

    VaApiImage *dst_image = _va_image_pool->AcquireBuffer();

    Image src_image;
    src_image.type = InferenceBackend::MemoryType::VAAPI;
    src_image.va_display = displayRaw();
    src_image.va_surface_id = src.GetVaSurfaceID();
    src_image.format = src.GetFormat();
    src_image.width = src.GetWidth();
    src_image.height = src.GetHeight();
    if (roi)
        src_image.rect = {roi->x, roi->y, roi->w, roi->h};
    else
        src_image.rect = {0, 0, src_image.width, src_image.height};

    _va_converter->Convert(src_image, *dst_image, _pre_proc_info);

    if (_out_memory_type == MemoryType::SYSTEM) {
        cv::Mat dst_mat;
        auto dst_mapped_image = dst_image->Map();
        InferenceBackend::Utils::ImageToMat(dst_mapped_image, dst_mat);
        InferenceBackend::Utils::MatToMultiPlaneImage(dst_mat, dst.GetFormat(), dst.GetWidth(), dst.GetHeight(),
                                                      dst.GetPlanes().data());

        dst_image->Unmap();
        _va_image_pool->ReleaseBuffer(dst_image);
    } else {
        auto info = new VaapiImageInfo{_va_image_pool, dst_image, {}};
        dst_image->sync = info->sync.get_future();
        gst_mini_object_set_qdata(&out_buffer->mini_object, g_quark_from_static_string("VaApiImage"), info,
                                  [](gpointer data) {
                                      auto info = reinterpret_cast<VaapiImageInfo *>(data);
                                      if (!info)
                                          return;
                                      if (info->image)
                                          info->image->Unmap();
                                      if (info->pool)
                                          info->pool->ReleaseBuffer(info->image);
                                      info->sync.set_value();
                                      delete info;
                                  });
    }
}

void VaapiPreProc::flush() {
    _va_image_pool->Flush();
}

size_t VaapiPreProc::output_size() const {
    if (_out_memory_type == MemoryType::VAAPI)
        return 0;
    return _output_tensor_info.GetSize();
}

#endif // ENABLE_VAAPI
