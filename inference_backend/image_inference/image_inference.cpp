/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "image_inference_async_preproc.h"
#include "openvino_image_inference.h"

ImageInference::Ptr ImageInference::make_shared(MemoryType /*type*/, const std::string &devices,
                                                const std::string &model, const unsigned int batch_size,
                                                const unsigned int nireq,
                                                const std::map<std::string, std::string> &config, Allocator *allocator,
                                                CallbackFunc callback) {
#ifdef HAVE_VAAPI
    auto it = config.find(KEY_PRE_PROCESSOR_TYPE);
    if (it != config.end() && it->second == "vaapi") {
        auto config2 = config;
        config2[KEY_PRE_PROCESSOR_TYPE] = "opencv"; // For data copying after vaapi pre-processing
        auto infer =
            std::make_shared<OpenVINOImageInference>(devices, model, batch_size, nireq, config2, allocator, callback);
        std::shared_ptr<InferenceBackend::PreProc> pre_proc;
        pre_proc.reset(PreProc::Create(PreProcessType::VAAPI));
        return std::make_shared<ImageInferenceAsyncPreProc>(infer, pre_proc, 5);
    }
#endif
    return std::make_shared<OpenVINOImageInference>(devices, model, batch_size, nireq, config, allocator, callback);
}
