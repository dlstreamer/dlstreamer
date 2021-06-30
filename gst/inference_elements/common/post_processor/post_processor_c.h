/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <processor_types.h>

typedef struct InferenceImpl InferenceImpl;

#ifdef __cplusplus
extern "C" {
#endif

PostProcessor *createPostProcessor(InferenceImpl *inference_impl, GvaBaseInference *base_inference);
void releasePostProcessor(PostProcessor *post_processor);

#ifdef __cplusplus
}
#endif
