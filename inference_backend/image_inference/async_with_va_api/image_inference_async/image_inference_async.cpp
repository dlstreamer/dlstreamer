/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
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

std::unique_ptr<VaApiImagePool> create_va_api_image_pool(VaApiImagePool::ImageInfo info, size_t pool_size,
                                                         VaApiContext *context) {

    // If ENABLE_GVA_FEATURES=vaapi-preproc-yuv set, then VA pipeline ends with scaled I420 image and I420->RGBP CSC
    // happens with OpenCV later.
    auto feature_toggler = std::unique_ptr<FeatureToggling::Runtime::RuntimeFeatureToggler>(
        new FeatureToggling::Runtime::RuntimeFeatureToggler());
    FeatureToggling::Runtime::EnvironmentVariableOptionsReader env_var_options_reader;
    feature_toggler->configure(env_var_options_reader.read("ENABLE_GVA_FEATURES"));
    if (feature_toggler->enabled(VaapiPreprocYUVToggle::id))
        info.format = FourCC::FOURCC_I420;
    else
        GVA_WARNING(VaapiPreprocYUVToggle::deprecation_message.c_str());

    std::unique_ptr<VaApiImagePool> pool =
        std::unique_ptr<VaApiImagePool>(new VaApiImagePool(context, pool_size, info));
    return pool;
}

VaApiImagePool::ImageInfo get_pool_image_info(const ImageInference::Ptr &inference) {
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = 0;
    int memory_type = 0;
    inference->GetModelImageInputInfo(width, height, batch_size, format, memory_type);
    VaApiImagePool::ImageInfo info = {.width = (uint32_t)width,
                                      .height = (uint32_t)height,
                                      .format = (FourCC)format,
                                      .memory_type = (MemoryType)memory_type};
    return info;
}

} // namespace

ImageInferenceAsync::ImageInferenceAsync(int image_pool_size, MemoryType image_memory_type)
    : _thread_pool(image_pool_size), _VA_IMAGE_POOL_SIZE(image_pool_size) {
    _va_context = std::unique_ptr<VaApiContext>(new VaApiContext());
    _va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(_va_context.get()));
    (void)image_memory_type;
}

void ImageInferenceAsync::Init() {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }
    _inference->Init();
}

void ImageInferenceAsync::SubmitInference(VaApiImage *va_api_image, IFrameBase::Ptr user_data,
                                          const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    auto deleter = [this, va_api_image](Image *img) {
        va_api_image->Unmap();
        this->_va_image_pool->ReleaseBuffer(va_api_image);
        delete img;
    };
    std::shared_ptr<Image> image = std::shared_ptr<Image>(new Image(va_api_image->Map()), deleter);
    user_data->SetImage(image);
    _inference->SubmitImage(*image, std::move(user_data), input_preprocessors);
}

void ImageInferenceAsync::SubmitImage(const Image &src_image, IFrameBase::Ptr user_data,
                                      const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    if (not _inference) {
        throw std::runtime_error("Image inference is not set");
    }

    auto inference_image_info = get_pool_image_info(_inference);
    if (!_va_image_pool) {
        _va_image_pool = create_va_api_image_pool(inference_image_info, _VA_IMAGE_POOL_SIZE, _va_context.get());
    }

    VaApiImage *dst_image = _va_image_pool->AcquireBuffer();
    _va_converter->Convert(src_image, *dst_image);

    dst_image->sync = _thread_pool.schedule([this, dst_image, user_data, input_preprocessors]() {
        SubmitInference(dst_image, user_data, input_preprocessors);
    });
}

const std::string &ImageInferenceAsync::GetModelName() const {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }
    return _inference->GetModelName();
}

void ImageInferenceAsync::GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                                 int &memory_type) const {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }
    _inference->GetModelImageInputInfo(width, height, batch_size, format, memory_type);
}

bool ImageInferenceAsync::IsQueueFull() {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }
    return _inference->IsQueueFull();
}

void ImageInferenceAsync::Flush() {
    if (_va_image_pool) {
        _va_image_pool->Flush();
    }
    if (_inference) {
        _inference->Flush();
    }
}

void ImageInferenceAsync::Close() {
    _inference->Close();
}

VaApiDisplay ImageInferenceAsync::GetVaDisplay() const {
    return _va_context->DisplayRaw();
}

void ImageInferenceAsync::SetInference(const ImageInference::Ptr &inference) {
    if (!inference) {
        throw std::runtime_error("Cannot initialize ImageInferenceAsync with empty (nullptr) inference");
    }
    _inference = inference;
}

ImageInferenceAsync::~ImageInferenceAsync() = default;
