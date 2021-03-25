/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <processor_types.h>

#ifdef __cplusplus
class InferenceImpl;
#else  /* __cplusplus */
typedef struct InferenceImpl InferenceImpl;
#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

PostProcessor *createInferencePostProcessor(const InferenceImpl *inference_impl);
void releaseInferencePostProcessor(PostProcessor *post_processor);

#ifdef __cplusplus
}
#endif
