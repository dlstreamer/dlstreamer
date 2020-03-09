/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async.h"
#include "vaapi_context.h"
#include "vaapi_converter.h"
#include "vaapi_images.h"

#include <future>
#include <tuple>
#include <utility>

using namespace InferenceBackend;

namespace {

std::tuple<std::unique_ptr<VaApiContext>, std::unique_ptr<VaApiImagePool>, std::unique_ptr<VaApiConverter>>
create_va_api_components(MemoryType memory_type, VADisplay display, const ImageInference::Ptr &inference,
                         size_t pool_size) {
    std::unique_ptr<VaApiContext> context = std::unique_ptr<VaApiContext>(new VaApiContext(memory_type, display));
    std::unique_ptr<VaApiConverter> converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(context.get()));
    int width = 0;
    int height = 0;
    int format = 0;
    int batch_size = 0;
    inference->GetModelInputInfo(&width, &height, &batch_size, &format);
    std::unique_ptr<VaApiImagePool> pool =
        std::unique_ptr<VaApiImagePool>(new VaApiImagePool(context.get(), pool_size, width, height, format));
    return std::make_tuple(std::move(context), std::move(pool), std::move(converter));
}

} // namespace

ImageInferenceAsync::ImageInferenceAsync(ImageInference::Ptr &&inference, int image_pool_size)
    : inference(inference), _thread_pool(image_pool_size), _VA_IMAGE_POOL_SIZE(image_pool_size) {
    if (!inference) {
        throw std::runtime_error("Cannot initialize ImageInferenceAsync with empty (nullptr) inference");
    }
}

void ImageInferenceAsync::SubmitInference(VaApiImage *va_api_image, IFramePtr user_data,
                                          std::function<void(Image &)> pre_processor) {
    Image image_sys = va_api_image->Map();
    inference->SubmitImage(image_sys, std::move(user_data), pre_processor);
    va_api_image->Unmap();
    _va_image_pool->ReleaseBuffer(va_api_image);
}

void ImageInferenceAsync::SubmitImage(const Image &image, IFramePtr user_data,
                                      std::function<void(Image &)> pre_processor) {
    if (!_va_context || _va_context->GetMemoryType() != image.type) {
        std::tie(_va_context, _va_image_pool, _va_converter) =
            create_va_api_components(image.type, image.va_display, inference, _VA_IMAGE_POOL_SIZE);
    }
    VaApiImage *dst_image = _va_image_pool->AcquireBuffer();
    _va_converter->Convert(image, *dst_image);
    dst_image->sync = _thread_pool.schedule(
        [this, dst_image, user_data, pre_processor]() { SubmitInference(dst_image, user_data, pre_processor); });
}

const std::string &ImageInferenceAsync::GetModelName() const {
    return inference->GetModelName();
}

void ImageInferenceAsync::GetModelInputInfo(int *width, int *height, int *batch_size, int *format) const {
    inference->GetModelInputInfo(width, height, batch_size, format);
}

bool ImageInferenceAsync::IsQueueFull() {
    return inference->IsQueueFull();
}

void ImageInferenceAsync::Flush() {
    if (_va_image_pool) {
        _va_image_pool->Flush();
    }
    inference->Flush();
}

void ImageInferenceAsync::Close() {
    inference->Close();
}
ImageInferenceAsync::~ImageInferenceAsync() = default;
