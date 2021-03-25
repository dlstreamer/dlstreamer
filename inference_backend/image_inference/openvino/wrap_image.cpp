/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "wrap_image.h"
#include "inference_backend/logger.h"
#include "utils.h"

#include "inference_backend/safe_arithmetic.h"
#include <ie_compound_blob.h>
#include <inference_backend/image.h>

#ifdef ENABLE_VPUX
#include <vpux/kmb_params.hpp>
#endif

#ifdef ENABLE_VAAPI
#include <gpu/gpu_context_api_va.hpp>
#endif

using namespace InferenceBackend;
using namespace InferenceEngine;

namespace WrapImageStrategy {

InferenceEngine::Blob::Ptr General::MakeSharedBlob(const InferenceBackend::Image &image,
                                                   const InferenceEngine::TensorDesc &tensor_desc,
                                                   size_t plane_num) const {
    return InferenceEngine::make_shared_blob<uint8_t>(tensor_desc, image.planes[plane_num]);
}

VPUX::VPUX(const InferenceEngine::RemoteContext::Ptr &remote_context) : remote_context(remote_context) {
}

InferenceEngine::Blob::Ptr VPUX::MakeSharedBlob(const InferenceBackend::Image &image,
                                                const InferenceEngine::TensorDesc &tensor_desc,
                                                size_t plane_num) const {
    InferenceEngine::Blob::Ptr blob;
#ifdef ENABLE_VPUX
    InferenceEngine::ParamMap params = {
        {InferenceEngine::KMB_PARAM_KEY(REMOTE_MEMORY_FD), image.dma_fd},
        {InferenceEngine::KMB_PARAM_KEY(MEM_HANDLE), reinterpret_cast<void *>(image.planes[plane_num])}};
    blob = remote_context->CreateBlob(tensor_desc, params);
#else
    UNUSED(image);
    UNUSED(tensor_desc);
    UNUSED(plane_num);
    assert("Trying to use WrapImageStrategy::VPUX when VPUX support was not enabled during build.");
#endif
    return blob;
}

} // namespace WrapImageStrategy

namespace {

inline int GetNumberChannels(int format) {
    switch (format) {
    case FourCC::FOURCC_BGRA:
    case FourCC::FOURCC_BGRX:
    case FourCC::FOURCC_RGBA:
    case FourCC::FOURCC_RGBX:
        return 4;
    case FourCC::FOURCC_BGR:
        return 3;
    }
    return 0;
}

Blob::Ptr BGRImageToBlob(const Image &image, const WrapImageStrategy::General &strategy) {
    int channels = GetNumberChannels(image.format);
    if (image.stride[0] != channels * image.width)
        throw std::runtime_error("Image is not dense");

    TensorDesc tensor_desc(Precision::U8, {1, (size_t)channels, (size_t)image.height, (size_t)image.width},
                           Layout::NHWC);

    auto image_blob = strategy.MakeSharedBlob(image, tensor_desc, 0);
    if (!image_blob)
        throw std::runtime_error("Failed to create blob for image plane");

    if (image.rect.width && image.rect.height) {
        ROI crop_roi(
            {0, (size_t)image.rect.x, (size_t)image.rect.y, (size_t)image.rect.width, (size_t)image.rect.height});
        image_blob = make_shared_blob(image_blob, crop_roi);
    }

    return image_blob;
}

Blob::Ptr NV12ImageToBlob(const Image &image, const WrapImageStrategy::General &strategy) {
    std::vector<size_t> NHWC = {0, 2, 3, 1};
    std::vector<size_t> dimOffsets = {0, 0, 0, 0};
    const size_t imageWidth = (size_t)image.width;
    const size_t imageHeight = (size_t)image.height;
    BlockingDesc memY({1, imageHeight, imageWidth, 1}, NHWC, 0, dimOffsets,
                      {image.offsets[1] + image.stride[0] * imageHeight / 2, image.stride[0], 1, 1});
    BlockingDesc memUV({1, imageHeight / 2, imageWidth / 2, 2}, NHWC, 0, dimOffsets,
                       {image.offsets[1] + image.stride[0] * imageHeight / 2, image.stride[1], 2, 1});
    TensorDesc planeY(Precision::U8, {1, 1, imageHeight, imageWidth}, memY);
    TensorDesc planeUV(Precision::U8, {1, 2, imageHeight / 2, imageWidth / 2}, memUV);

    auto blobY = strategy.MakeSharedBlob(image, planeY, 0);
    auto blobUV = strategy.MakeSharedBlob(image, planeUV, 1);
    if (!blobY || !blobUV)
        throw std::runtime_error("Failed to create blob for Y or UV plane");

    ROI crop_roi_y({
        0,
        (size_t)((image.rect.x & 0x1) ? image.rect.x - 1 : image.rect.x),
        (size_t)((image.rect.y & 0x1) ? image.rect.y - 1 : image.rect.y),
        (size_t)((image.rect.width & 0x1) ? image.rect.width - 1 : image.rect.width),
        (size_t)((image.rect.height & 0x1) ? image.rect.height - 1 : image.rect.height),
    });
    ROI crop_roi_uv({0, (size_t)image.rect.x / 2, (size_t)image.rect.y / 2, (size_t)image.rect.width / 2,
                     (size_t)image.rect.height / 2});

    Blob::Ptr y_plane_with_roi = make_shared_blob(blobY, crop_roi_y);
    Blob::Ptr uv_plane_with_roi = make_shared_blob(blobUV, crop_roi_uv);
    Blob::Ptr nv12Blob = make_shared_blob<NV12Blob>(y_plane_with_roi, uv_plane_with_roi);
    return nv12Blob;
}

Blob::Ptr I420ImageToBlob(const Image &image, const WrapImageStrategy::General &strategy) {
    std::vector<size_t> NHWC = {0, 2, 3, 1};
    std::vector<size_t> dimOffsets = {0, 0, 0, 0};
    const size_t image_width = static_cast<size_t>(image.width);
    const size_t image_height = static_cast<size_t>(image.height);
    BlockingDesc memY({1, image_height, image_width, 1}, NHWC, 0, dimOffsets,
                      {image.offsets[1] + image_height * image.stride[0] / 2, image.stride[0], 1, 1});
    BlockingDesc memU({1, image_height / 2, image_width / 2, 1}, NHWC, 0, dimOffsets,
                      {image.offsets[1] + image_height * image.stride[0] / 2, image.stride[1], 1, 1});
    BlockingDesc memV({1, image_height / 2, image_width / 2, 1}, NHWC, 0, dimOffsets,
                      {image.offsets[1] + image_height * image.stride[0] / 2, image.stride[2], 1, 1});

    TensorDesc Y_plane_desc(Precision::U8, {1, 1, image_height, image_width}, memY);
    TensorDesc U_plane_desc(Precision::U8, {1, 1, image_height / 2, image_width / 2}, memU);
    TensorDesc V_plane_desc(Precision::U8, {1, 1, image_height / 2, image_width / 2}, memV);
    if (!image.planes[0] or !image.planes[1] or !image.planes[2])
        throw std::invalid_argument("Planes number for I420 image is less than 3");

    auto Y_plane_blob = strategy.MakeSharedBlob(image, Y_plane_desc, 0);
    auto U_plane_blob = strategy.MakeSharedBlob(image, U_plane_desc, 1);
    auto V_plane_blob = strategy.MakeSharedBlob(image, V_plane_desc, 2);
    if (!Y_plane_blob || !U_plane_blob || !V_plane_blob)
        throw std::runtime_error("Failed to create blob for Y, or U, or V plane");

    ROI Y_roi({
        0,
        (size_t)((image.rect.x & 0x1) ? image.rect.x - 1 : image.rect.x),
        (size_t)((image.rect.y & 0x1) ? image.rect.y - 1 : image.rect.y),
        (size_t)((image.rect.width & 0x1) ? image.rect.width - 1 : image.rect.width),
        (size_t)((image.rect.height & 0x1) ? image.rect.height - 1 : image.rect.height),
    });
    ROI U_V_roi({0, (size_t)image.rect.x / 2, (size_t)image.rect.y / 2, (size_t)image.rect.width / 2,
                 (size_t)image.rect.height / 2});

    Blob::Ptr Y_plane_with_roi = make_shared_blob(Y_plane_blob, Y_roi);
    Blob::Ptr U_plane_with_roi = make_shared_blob(U_plane_blob, U_V_roi);
    Blob::Ptr V_plane_with_roi = make_shared_blob(V_plane_blob, U_V_roi);
    Blob::Ptr i420_blob = make_shared_blob<I420Blob>(Y_plane_with_roi, U_plane_with_roi, V_plane_with_roi);
    return i420_blob;
}

} // namespace

Blob::Ptr WrapImageToBlob(const Image &image, const WrapImageStrategy::General &strategy) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    try {
        switch (image.format) {
        case FourCC::FOURCC_BGRA:
        case FourCC::FOURCC_BGRX:
        case FourCC::FOURCC_RGBA:
        case FourCC::FOURCC_RGBX:
        case FourCC::FOURCC_BGR:
            return BGRImageToBlob(image, strategy);

        case FourCC::FOURCC_NV12:
            return NV12ImageToBlob(image, strategy);

        case FourCC::FOURCC_I420:
            return I420ImageToBlob(image, strategy);

        default:
            throw std::logic_error("Unsupported image type");
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to wrap image to InferenceEngine blob"));
    }
}

#ifdef ENABLE_VAAPI
Blob::Ptr WrapImageToBlob(const Image &image, const RemoteContext::Ptr &remote_context) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    try {
        Blob::Ptr blob = nullptr;
        switch (image.format) {
        case FourCC::FOURCC_NV12: {
            if (image.va_surface_id == 0xffffffff)
                throw std::runtime_error("Incorrect va surface");
            if (!remote_context)
                throw std::runtime_error("Incorrect context, can not create surface");

            blob = gpu::make_shared_blob_nv12(image.height, image.width, remote_context, image.va_surface_id);
            break;
        }
        default:
            throw std::invalid_argument("Unsupported image type");
        }
        return blob;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to wrap image to InferenceEngine blob"));
    }
}
#endif
