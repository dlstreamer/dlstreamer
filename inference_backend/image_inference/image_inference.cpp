/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async/image_inference_async.h"
#include "openvino_image_inference.h"
#include "utils.h"

using namespace InferenceBackend;

ImageInference::Ptr ImageInference::make_shared(MemoryType input_image_memory_type, const std::string &model,
                                                const std::map<std::string, std::map<std::string, std::string>> &config,
                                                Allocator *allocator, CallbackFunc callback,
                                                ErrorHandlingFunc error_handler, const std::string &device_name) {
    MemoryType inference_memory_type;
    ImagePreprocessorType image_preprocessor_type = ImagePreprocessorType::INVALID;
    if (input_image_memory_type == MemoryType::VAAPI or input_image_memory_type == MemoryType::DMA_BUFFER) {
        const std::map<std::string, std::string> &base = config.at(KEY_BASE);
        auto it = base.find(KEY_PRE_PROCESSOR_TYPE);
        if (it != base.end()) {
            image_preprocessor_type = static_cast<ImagePreprocessorType>(std::stoi(it->second));
            switch (image_preprocessor_type) {
            case ImagePreprocessorType::VAAPI_SYSTEM:
                inference_memory_type = MemoryType::SYSTEM;
                break;
            case ImagePreprocessorType::VAAPI_SURFACE_SHARING:
                inference_memory_type = MemoryType::VAAPI;
                break;
            default:
                throw std::runtime_error(
                    "Incorrect pre-process-backend, should be equal vaapi or vaapi-surface-sharing");
            }
        }
    }

    ImageInference::Ptr image_inference;
    switch (input_image_memory_type) {
#if defined(ENABLE_VAAPI)
    case MemoryType::DMA_BUFFER:
    case MemoryType::VAAPI: {
        std::shared_ptr<ImageInferenceAsync> image_inference_async;
        const int image_pool_size = 5;
        image_inference = image_inference_async =
            std::make_shared<ImageInferenceAsync>(image_pool_size, input_image_memory_type);
        if (image_preprocessor_type == ImagePreprocessorType::VAAPI_SURFACE_SHARING) {
            void *display = image_inference_async->GetVaDisplay();
            auto infer = std::make_shared<OpenVINOImageInference>(model, config, display, callback, error_handler,
                                                                  inference_memory_type, device_name);
            image_inference_async->SetInference(infer);
            infer->Init();
            break;
        }
        auto infer = std::make_shared<OpenVINOImageInference>(model, config, allocator, callback, error_handler,
                                                              inference_memory_type, device_name);
        image_inference_async->SetInference(infer);
        infer->Init();
        break;
    }
#else
        UNUSED(inference_memory_type);
        UNUSED(image_preprocessor_type);
#endif
    case MemoryType::SYSTEM: {
        image_inference = std::make_shared<OpenVINOImageInference>(model, config, allocator, callback, error_handler,
                                                                   input_image_memory_type, device_name);
        image_inference->Init();
        break;
    }
    default:
        throw std::invalid_argument("Unsupported memory type");
    }

    return image_inference;
}
