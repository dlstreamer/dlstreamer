/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_preproc.hpp"

#ifdef ENABLE_VAAPI

#include <opencv_utils/opencv_utils.h>
#include <utils.h>
#include <vaapi_converter.h>
#include <vaapi_images.h>

using namespace InferenceBackend;

namespace {

constexpr size_t _VA_IMAGE_POOL_SIZE = 5;

struct VaapiImageInfo {
    std::shared_ptr<InferenceBackend::VaApiImagePool> pool;
    VaApiImage *image;
};

} // namespace

VaapiPreProc::VaapiPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info,
                           const TensorCaps &output_tensor_info, InferenceBackend::FourCC format,
                           MemoryType out_memory_type)
    : _input_video_info(input_video_info), _output_tensor_info(output_tensor_info), _out_memory_type(out_memory_type),
      _va_context(new VaApiContext(display)), _va_converter(new VaApiConverter(_va_context.get())) {
    uint32_t batch = _output_tensor_info.HasBatchSize() ? _output_tensor_info.GetBatchSize() : 1;

    _va_image_pool.reset(
        new VaApiImagePool(_va_context.get(), _VA_IMAGE_POOL_SIZE,
                           VaApiImagePool::ImageInfo{static_cast<uint32_t>(output_tensor_info.GetDimension(2)),
                                                     static_cast<uint32_t>(output_tensor_info.GetDimension(1)), batch,
                                                     format, _out_memory_type}));
}

VaapiPreProc::~VaapiPreProc() = default;

void VaapiPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer) {
    FrameData src;
    src.Map(in_buffer, _input_video_info, InferenceBackend::MemoryType::VAAPI, GST_MAP_READ);

    FrameData dst;
    // TODO: always 3-channel output for vaapi preproc
    dst.Map(out_buffer, _output_tensor_info, GST_MAP_WRITE, 3);

    VaApiImage *dst_image = _va_image_pool->AcquireBuffer();

    Image src_image;
    src_image.type = InferenceBackend::MemoryType::VAAPI;
    src_image.va_display = src.GetVaMemInfo().va_display;
    src_image.va_surface_id = src.GetVaMemInfo().va_surface_id;
    src_image.format = src.GetFormat();
    src_image.width = src.GetWidth();
    src_image.height = src.GetHeight();

    _va_converter->Convert(src_image, *dst_image);

    if (_out_memory_type == MemoryType::SYSTEM) {
        cv::Mat dst_mat;
        auto dst_mapped_image = dst_image->Map();
        InferenceBackend::Utils::ImageToMat(dst_mapped_image, dst_mat);

        uint8_t *dst_planes[MAX_PLANES_NUM];
        for (guint i = 0; i < dst.GetPlanesNum(); i++)
            dst_planes[i] = dst.GetPlane(i);
        InferenceBackend::Utils::MatToMultiPlaneImage(dst_mat, dst.GetFormat(), dst.GetWidth(), dst.GetHeight(),
                                                      dst_planes);

        dst_image->Unmap();
        _va_image_pool->ReleaseBuffer(dst_image);
    } else {
        auto va_image = dst_image->Map();
        gst_mini_object_set_qdata(&out_buffer->mini_object, g_quark_from_static_string("VADisplay"),
                                  va_image.va_display, nullptr);
        gst_mini_object_set_qdata(&out_buffer->mini_object, g_quark_from_static_string("VASurfaceID"),
                                  reinterpret_cast<gpointer>(va_image.va_surface_id), nullptr);
        gst_mini_object_set_qdata(&out_buffer->mini_object, g_quark_from_static_string("VaApiImage"),
                                  new VaapiImageInfo{_va_image_pool, dst_image}, [](gpointer data) {
                                      auto info = reinterpret_cast<VaapiImageInfo *>(data);
                                      if (!info)
                                          return;
                                      if (info->image)
                                          info->image->Unmap();
                                      if (info->pool)
                                          info->pool->ReleaseBuffer(info->image);
                                      delete info;
                                  });
    }
}

void VaapiPreProc::process(GstBuffer *) {
    throw std::runtime_error("VaapiPreProc: In-place processing is not supported");
}

#endif // ENABLE_VAAPI
