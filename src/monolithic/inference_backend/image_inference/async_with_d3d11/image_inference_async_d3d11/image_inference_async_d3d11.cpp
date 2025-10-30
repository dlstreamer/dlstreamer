/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/input_image_layer_descriptor.h"

#include "image_inference_async_d3d11.h"
#include "inference_backend/image_inference.h"
#include "utils.h"

#include "d3d11_context.h"
#include "d3d11_converter.h"
#include "d3d11_images.h"

#include "safe_arithmetic.hpp"

#include <future>
#include <string>
#include <tuple>
#include <utility>

using namespace InferenceBackend;

namespace {

std::unique_ptr<D3D11ImagePool> create_d3d11_image_pool(D3D11ImagePool::ImageInfo info, size_t pool_size,
                                                        D3D11Context *context, float vdbox_sfc_pipe_part) {

    D3D11ImagePool::SizeParams size_params;
    // vdbox_sfc_pipe_part is checked below to be in range [0,1]
    size_params.num_fast = vdbox_sfc_pipe_part * pool_size;
    size_params.num_default = pool_size - size_params.num_fast;
    return std::unique_ptr<D3D11ImagePool>(new D3D11ImagePool(context, size_params, info));
}

D3D11ImagePool::ImageInfo get_pool_image_info(const ImageInference::Ptr &inference) {
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = 0;
    int memory_type = 0;
    inference->GetModelImageInputInfo(width, height, batch_size, format, memory_type);
    D3D11ImagePool::ImageInfo info = {.width = safe_convert<uint32_t>(width),
                                      .height = safe_convert<uint32_t>(height),
                                      .batch = safe_convert<uint32_t>(batch_size),
                                      .format = static_cast<FourCC>(format),
                                      .memory_type = static_cast<MemoryType>(memory_type)};
    return info;
}

const InputImageLayerDesc::Ptr
getImagePreProcInfo(const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) {
    const auto image_it = input_preprocessors.find("image");
    if (image_it != input_preprocessors.cend()) {
        const auto description = image_it->second;
        if (description) {
            return description->input_image_preroc_params;
        }
    }

    return nullptr;
}

} // namespace

ImageInferenceAsyncD3D11::ImageInferenceAsyncD3D11(const InferenceBackend::InferenceConfig &config,
                                                   dlstreamer::ContextPtr d3d11_context, ImageInference::Ptr inference)
    : _inference(inference) {

    const auto &pre_process_config = config.at(KEY_PRE_PROCESSOR);
    auto thread_pool_size_it = pre_process_config.find(KEY_D3D11_THREAD_POOL_SIZE);
    size_t thread_pool_size = thread_pool_size_it == pre_process_config.end()
                                  ? DEFAULT_THREAD_POOL_SIZE
                                  : std::stoull(thread_pool_size_it->second);

    _thread_pool.reset(new D3D11::ThreadPool(thread_pool_size));

    _d3d11_context = std::unique_ptr<D3D11Context>(new D3D11Context(d3d11_context));
    _d3d11_converter = std::unique_ptr<D3D11Converter>(new D3D11Converter(_d3d11_context.get()));

    auto inference_image_info = get_pool_image_info(_inference);

    size_t inference_buffers = safe_mul(safe_convert<size_t>(inference_image_info.batch), _inference->GetNireq());
    size_t pool_threads = static_cast<size_t>(thread_pool_size) * 3;
    size_t image_pool_size = std::max(inference_buffers, pool_threads);

    GVA_INFO("D3D11 async preprocessing configuration:");
    GVA_INFO("-- Inference buffers needed: %lu (nireq=%u, batch=%u)", inference_buffers, _inference->GetNireq(),
             inference_image_info.batch);
    GVA_INFO("-- Thread pool size: %lu", thread_pool_size);
    GVA_INFO("-- D3D11 image pool size: %lu (ensures enough buffering for pipelining)", image_pool_size);

    _d3d11_image_pool = create_d3d11_image_pool(inference_image_info, image_pool_size, _d3d11_context.get(), 0.0f);
}

void ImageInferenceAsyncD3D11::SubmitInference(D3D11Image *d3d11_image, IFrameBase::Ptr frame,
                                               const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    if (!d3d11_image)
        throw std::invalid_argument("Invalid D3D11Image object");
    if (!frame)
        throw std::invalid_argument("Invalid frame object");

    auto deleter = [this, d3d11_image](Image *img) {
        delete img;
        try {
            d3d11_image->Unmap();
            this->_d3d11_image_pool->ReleaseBuffer(d3d11_image);
        } catch (const std::exception &e) {
            GVA_ERROR("Couldn't release D3D11Image: %s", e.what());
        }
    };

    Image mapped_image = d3d11_image->Map();
    frame->SetImage(std::shared_ptr<Image>(new Image(mapped_image), deleter));
    _inference->SubmitImage(std::move(frame), input_preprocessors);
}

void ImageInferenceAsyncD3D11::SubmitImage(IFrameBase::Ptr frame,
                                           const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    assert(frame && "Expected valid IFrameBase pointer");
    D3D11Image *d3d11_image = _d3d11_image_pool->AcquireBuffer();

    try {
        _d3d11_converter->Convert(*frame->GetImage(), *d3d11_image, getImagePreProcInfo(input_preprocessors),
                                  frame->GetImageTransformationParams());
    } catch (std::exception &e) {
        GVA_ERROR("D3D11 SubmitImage: Convert failed: %s", e.what());
        _d3d11_image_pool->ReleaseBuffer(d3d11_image);
        std::throw_with_nested(std::runtime_error("Unable to convert image using D3D11"));
    }

    d3d11_image->sync = _thread_pool->schedule([this, d3d11_image, f = std::move(frame), input_preprocessors]() {
        try {
            SubmitInference(d3d11_image, std::move(f), input_preprocessors);
        } catch (const std::exception &e) {
            GVA_ERROR("D3D11 async task exception: %s", e.what());
            throw;
        }
    });
}

const std::string &ImageInferenceAsyncD3D11::GetModelName() const {
    return _inference->GetModelName();
}

size_t ImageInferenceAsyncD3D11::GetBatchSize() const {
    return _inference->GetBatchSize();
}

size_t ImageInferenceAsyncD3D11::GetNireq() const {
    return _inference->GetNireq();
}

void ImageInferenceAsyncD3D11::GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                                      int &memory_type) const {
    _inference->GetModelImageInputInfo(width, height, batch_size, format, memory_type);
}

std::map<std::string, std::vector<size_t>> ImageInferenceAsyncD3D11::GetModelInputsInfo() const {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }

    return _inference->GetModelInputsInfo();
}

std::map<std::string, std::vector<size_t>> ImageInferenceAsyncD3D11::GetModelOutputsInfo() const {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }

    return _inference->GetModelOutputsInfo();
}

std::map<std::string, GstStructure *> ImageInferenceAsyncD3D11::GetModelInfoPostproc() const {
    if (not _inference) {
        throw std::runtime_error("Inference not set");
    }

    return _inference->GetModelInfoPostproc();
}

bool ImageInferenceAsyncD3D11::IsQueueFull() {
    return _inference->IsQueueFull();
}

void ImageInferenceAsyncD3D11::Flush() {
    if (_d3d11_image_pool) {
        _d3d11_image_pool->Flush();
    }
    if (_inference) {
        _inference->Flush();
    }
}

void ImageInferenceAsyncD3D11::Close() {
    _inference->Close();
}

ImageInferenceAsyncD3D11::~ImageInferenceAsyncD3D11() = default;
