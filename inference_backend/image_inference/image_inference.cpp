/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async/image_inference_async.h"
#include "openvino_image_inference.h"
#include "utils.h"

using namespace InferenceBackend;

namespace {

ImagePreprocessorType getPreProcType(const std::map<std::string, std::string> &base_config) {
    auto it = base_config.find(KEY_PRE_PROCESSOR_TYPE);
    if (it == base_config.end())
        throw std::runtime_error("Image pre-processor type is not set");
    return static_cast<ImagePreprocessorType>(std::stoi(it->second));
}

} // namespace

ImageInference::Ptr ImageInference::make_shared(MemoryType input_image_memory_type, const InferenceConfig &config,
                                                Allocator *allocator, CallbackFunc callback,
                                                ErrorHandlingFunc error_handler, dlstreamer::ContextPtr context) {
    bool async_mode = false;
    // Resulted memory type that will be used for inference
    MemoryType memory_type_to_use = MemoryType::ANY;

    switch (input_image_memory_type) {
    case MemoryType::SYSTEM:
        // Nothing special for system memory.
        memory_type_to_use = input_image_memory_type;
        break;

    case MemoryType::DMA_BUFFER:
    case MemoryType::VAAPI: {
        async_mode = true;

        // The display must present.
        if (!context)
            throw std::invalid_argument("Null context provided (VaApiContext is expected)");

        ImagePreprocessorType preproc_type = getPreProcType(config.at(KEY_BASE));
        switch (preproc_type) {
        case ImagePreprocessorType::VAAPI_SYSTEM:
            memory_type_to_use = MemoryType::SYSTEM;
            // For OV instance VADisplay is not needed in this case.
            break;
        case ImagePreprocessorType::VAAPI_SURFACE_SHARING:
            memory_type_to_use = MemoryType::VAAPI;
            break;
        default:
            throw std::runtime_error("Incorrect pre-process-backend, should be equal vaapi or vaapi-surface-sharing");
        }
        break;
    }

    default:
        throw std::invalid_argument("Unsupported memory type");
    }

    auto ov_inference = std::make_shared<OpenVINOImageInference>(config, allocator, context, callback, error_handler,
                                                                 memory_type_to_use);

    ImageInference::Ptr result_inference;
    if (async_mode) {
#ifdef ENABLE_VAAPI
        result_inference = std::make_shared<ImageInferenceAsync>(config, context, std::move(ov_inference));
#endif
    } else {
        result_inference = std::move(ov_inference);
    }

    return result_inference;
}
