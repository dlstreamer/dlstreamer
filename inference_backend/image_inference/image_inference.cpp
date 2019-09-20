/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async/image_inference_async.h"
#include "openvino_image_inference.h"

using namespace InferenceBackend;

ImageInference::Ptr ImageInference::make_shared(MemoryType /*type*/, const std::string &devices,
                                                const std::string &model, const unsigned int batch_size,
                                                const unsigned int nireq,
                                                const std::map<std::string, std::string> &config, Allocator *allocator,
                                                CallbackFunc callback) {
#ifdef HAVE_VAAPI
    auto it = config.find(KEY_PRE_PROCESSOR_TYPE);
    if (it != config.end() && it->second == "vaapi") {
        auto infer =
            std::make_shared<OpenVINOImageInference>(devices, model, batch_size, nireq, config, allocator, callback);
        return std::make_shared<ImageInferenceAsync>(std::move(infer), 5);
    }
#endif
    return std::make_shared<OpenVINOImageInference>(devices, model, batch_size, nireq, config, allocator, callback);
}
