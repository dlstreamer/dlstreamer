/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "detection_post_processors_c.h"
#include "detection_post_processor.h"

#include "utils.h"
#include <inference_backend/logger.h>

PostProcessor *createDetectionPostProcessor(InferenceImpl *inference_impl) {
    if (inference_impl == nullptr) {
        GVA_WARNING("InferenceImpl is null. Creating of detection post processor is imposible");
        return nullptr;
    }

    PostProcessor *post_processor = nullptr;
    try {
        post_processor = new DetectionPlugin::DetectionPostProcessor(inference_impl);
    } catch (const std::exception &e) {
        GVA_ERROR(Utils::createNestedErrorMsg(e).c_str());
    }
    return post_processor;
}
void releaseDetectionPostProcessor(PostProcessor *post_processor) {
    if (post_processor)
        delete post_processor;
}
