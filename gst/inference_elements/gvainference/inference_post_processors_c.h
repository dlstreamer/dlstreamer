/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <processor_types.h>

typedef struct InferenceImpl InferenceImpl;

#ifdef __cplusplus
extern "C" {
#endif

PostProcessor *createInferencePostProcessor(const InferenceImpl *inference_impl);
void releaseInferencePostProcessor(PostProcessor *post_processor);

#ifdef __cplusplus
}
#endif
