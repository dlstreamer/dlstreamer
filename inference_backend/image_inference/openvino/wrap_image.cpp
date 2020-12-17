/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "wrap_image.h"
#include "inference_backend/logger.h"

#include "inference_backend/safe_arithmetic.h"
#include <ie_compound_blob.h>
#include <inference_backend/image.h>

#ifdef ENABLE_VAAPI
#include <gpu/gpu_context_api_va.hpp>
#endif

using namespace InferenceBackend;
using namespace InferenceEngine;
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
} // namespace

Blob::Ptr WrapImageToBlob(const Image &image) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    try {
        Blob::Ptr blob;
        std::vector<size_t> NHWC = {0, 2, 3, 1};
        std::vector<size_t> dimOffsets = {0, 0, 0, 0};
        switch (image.format) {
        case FourCC::FOURCC_BGRA:
        case FourCC::FOURCC_BGRX:
        case FourCC::FOURCC_RGBA:
        case FourCC::FOURCC_RGBX:
        case FourCC::FOURCC_BGR: {
            int channels = GetNumberChannels(image.format);
            if (image.stride[0] != channels * image.width)
                throw std::runtime_error("Image is not dense");

            TensorDesc tensor_desc(Precision::U8, {1, (size_t)channels, (size_t)image.height, (size_t)image.width},
                                   Layout::NHWC);
            Blob::Ptr image_blob = make_shared_blob<uint8_t>(tensor_desc, image.planes[0]);

            if (image.rect.width && image.rect.height) {
                ROI crop_roi({0, (size_t)image.rect.x, (size_t)image.rect.y, (size_t)image.rect.width,
                              (size_t)image.rect.height});
                image_blob = make_shared_blob(image_blob, crop_roi);
            }

            blob = image_blob;
            break;
        }
        case FourCC::FOURCC_NV12: {
            const size_t imageWidth = (size_t)image.width;
            const size_t imageHeight = (size_t)image.height;

            BlockingDesc memY({1, imageHeight, imageWidth, 1}, NHWC, 0, dimOffsets,
                              {image.offsets[1] + image.stride[0] * imageHeight / 2, image.stride[0], 1, 1});
            BlockingDesc memUV({1, imageHeight / 2, imageWidth / 2, 2}, NHWC, 0, dimOffsets,
                               {image.offsets[1] + image.stride[0] * imageHeight / 2, image.stride[1], 2, 1});

            TensorDesc planeY(Precision::U8, {1, 1, imageHeight, imageWidth}, memY);
            TensorDesc planeUV(Precision::U8, {1, 2, imageHeight / 2, imageWidth / 2}, memUV);

            ROI crop_roi_y({
                0,
                (size_t)((image.rect.x & 0x1) ? image.rect.x - 1 : image.rect.x),
                (size_t)((image.rect.y & 0x1) ? image.rect.y - 1 : image.rect.y),
                (size_t)((image.rect.width & 0x1) ? image.rect.width - 1 : image.rect.width),
                (size_t)((image.rect.height & 0x1) ? image.rect.height - 1 : image.rect.height),
            });

            ROI crop_roi_uv({0, (size_t)image.rect.x / 2, (size_t)image.rect.y / 2, (size_t)image.rect.width / 2,
                             (size_t)image.rect.height / 2});

            Blob::Ptr blobY = make_shared_blob<uint8_t>(planeY, image.planes[0]);
            Blob::Ptr blobUV = make_shared_blob<uint8_t>(planeUV, image.planes[1]);

            Blob::Ptr y_plane_with_roi = make_shared_blob(blobY, crop_roi_y);
            Blob::Ptr uv_plane_with_roi = make_shared_blob(blobUV, crop_roi_uv);

            Blob::Ptr nv12Blob = make_shared_blob<NV12Blob>(y_plane_with_roi, uv_plane_with_roi);

            blob = nv12Blob;
            break;
        }
        case FourCC::FOURCC_I420: {
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
            if (!image.planes[0] or !image.planes[1] or !image.planes[2]) {
                throw std::invalid_argument("Planes number for I420 image is less than 3");
            }

            Blob::Ptr Y_plane_blob = make_shared_blob<uint8_t>(Y_plane_desc, image.planes[0]);
            Blob::Ptr U_plane_blob = make_shared_blob<uint8_t>(U_plane_desc, image.planes[1]);
            Blob::Ptr V_plane_blob = make_shared_blob<uint8_t>(V_plane_desc, image.planes[2]);

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

            blob = i420_blob;
            break;
        }
        default:
            throw std::logic_error("Unsupported image type");
        }
        return blob;
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
