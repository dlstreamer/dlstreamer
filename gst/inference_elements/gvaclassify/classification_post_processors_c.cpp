/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classification_post_processors_c.h"

#include "classification_post_processors.h"

#include "utils.h"
#include <inference_backend/logger.h>

PostProcessor *createClassificationPostProcessor(InferenceImpl *inference_impl) {
    if (inference_impl == nullptr) {
        GVA_WARNING("InferenceImpl is null. Creating of classification post processor is imposible");
        return nullptr;
    }
    PostProcessor *post_processor = nullptr;
    try {
        post_processor = new ClassificationPlugin::ClassificationPostProcessor(inference_impl);
    } catch (const std::exception &e) {
        GVA_ERROR(Utils::createNestedErrorMsg(e).c_str());
    }
    return post_processor;
}

void releaseClassificationPostProcessor(PostProcessor *post_processor) {
    if (post_processor)
        delete post_processor;
}
