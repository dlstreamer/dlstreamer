/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_post_processors_c.h"

#include "inference_post_processor.h"

#include "utils.h"
#include <inference_backend/logger.h>

PostProcessor *createInferencePostProcessor(const InferenceImpl *inference_impl) {
    if (inference_impl == nullptr) {
        GVA_WARNING("InferenceImpl is null. Creating of inference post processor is imposible");
        return nullptr;
    }
    PostProcessor *post_processor = nullptr;
    try {
        post_processor = new InferencePlugin::InferencePostProcessor(inference_impl);
    } catch (const std::exception &e) {
        GVA_ERROR(Utils::createNestedErrorMsg(e).c_str());
    }
    return post_processor;
}

void releaseInferencePostProcessor(PostProcessor *post_processor) {
    if (post_processor)
        delete post_processor;
}
