/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <config.h>

// TODO: define default values

#define GVA_TENSOR_CAPS                                                                                                \
    "application/tensor, "                                                                                             \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NCHW, NHWC, CHW, NC };"
// "dims = (string)"

#ifdef ENABLE_VAAPI
#define GVA_VAAPI_TENSOR_CAPS                                                                                          \
    "application/tensor(memory:VASurface), "                                                                           \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NCHW, NHWC, CHW, NC };"
// "dims = (string)"
#else
#define GVA_VAAPI_TENSOR_CAPS ""
#endif

#define GVA_TENSORS_CAPS "application/tensors;"
// descs = (array)

#ifdef ENABLE_VAAPI
#define GVA_VAAPI_TENSORS_CAPS "application/tensors(memory:VASurface);"
// descs = (array)
#else
#define GVA_VAAPI_TENSORS_CAPS ""
#endif
