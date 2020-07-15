/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "environment_variable_options_reader.h"
#include "feature_toggling/ifeature_toggle.h"
#include "runtime_feature_toggler.h"

#include "image_inference_async.h"
#include "vaapi_context.h"
#include "vaapi_converter.h"
#include "vaapi_images.h"

#include <future>
#include <tuple>
#include <utility>

using namespace InferenceBackend;

namespace {

CREATE_FEATURE_TOGGLE(VaapiPreprocYUVToggle, "vaapi-preproc-yuv",
                      "Vaapi pre-proc with RGBP output may be not high-performant on some systems. Please set "
                      "environment variable ENABLE_GVA_FEATURES=vaapi-preproc-yuv to enable I420 output for vaapi "
                      "pre-proc and see if it enables better performance. ")

std::tuple<std::unique_ptr<VaApiContext>, std::unique_ptr<VaApiImagePool>, std::unique_ptr<VaApiConverter>>
create_va_api_components(MemoryType memory_type, VADisplay display, const ImageInference::Ptr &inference,
                         size_t pool_size) {
    std::unique_ptr<VaApiContext> context = std::unique_ptr<VaApiContext>(new VaApiContext(memory_type, display));
    std::unique_ptr<VaApiConverter> converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(context.get()));
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = 0;
    inference->GetModelImageInputInfo(width, height, batch_size, format);

    // If ENABLE_GVA_FEATURES=vaapi-preproc-yuv set, then VA pipeline ends with scaled I420 image and I420->RGBP CSC
    // happens with OpenCV later This is verified to improve performance of VASurface-based pipelines on Atom + MyriadX
    // systems
    auto feature_toggler = std::unique_ptr<FeatureToggling::Runtime::RuntimeFeatureToggler>(
        new FeatureToggling::Runtime::RuntimeFeatureToggler());
    FeatureToggling::Runtime::EnvironmentVariableOptionsReader env_var_options_reader;
    feature_toggler->configure(env_var_options_reader.read("ENABLE_GVA_FEATURES"));
    if (feature_toggler->enabled(VaapiPreprocYUVToggle::id))
        format = FourCC::FOURCC_I420;
    else
        GVA_WARNING(VaapiPreprocYUVToggle::deprecation_message.c_str());

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
                                          const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    Image image_sys = va_api_image->Map();
    inference->SubmitImage(image_sys, std::move(user_data), input_preprocessors);
    va_api_image->Unmap();
    _va_image_pool->ReleaseBuffer(va_api_image);
}

void ImageInferenceAsync::SubmitImage(const Image &image, IFramePtr user_data,
                                      const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    if (!_va_context || _va_context->GetMemoryType() != image.type) {
        std::tie(_va_context, _va_image_pool, _va_converter) =
            create_va_api_components(image.type, image.va_display, inference, _VA_IMAGE_POOL_SIZE);
    }
    VaApiImage *dst_image = _va_image_pool->AcquireBuffer();
    _va_converter->Convert(image, *dst_image);
    dst_image->sync = _thread_pool.schedule([this, dst_image, user_data, input_preprocessors]() {
        SubmitInference(dst_image, user_data, input_preprocessors);
    });
}

const std::string &ImageInferenceAsync::GetModelName() const {
    return inference->GetModelName();
}

void ImageInferenceAsync::GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format) const {
    inference->GetModelImageInputInfo(width, height, batch_size, format);
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
