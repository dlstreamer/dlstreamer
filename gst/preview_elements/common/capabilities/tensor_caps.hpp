/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <config.h>

// TODO: define default values

#define GVA_TENSOR_MEDIA_NAME "other/tensors"

#define GVA_TENSORS_CAPS GVA_TENSOR_MEDIA_NAME ";"

#ifdef ENABLE_VAAPI
#define GVA_VAAPI_TENSORS_CAPS GVA_TENSOR_MEDIA_NAME "(memory:VASurface);"
#else
#define GVA_VAAPI_TENSORS_CAPS ""
#endif

#define GVA_TENSOR_CAPS_MAKE(types, dims)                                                                              \
    "" GVA_TENSOR_MEDIA_NAME ","                                                                                       \
    "types = " types ","                                                                                               \
    "dimensions = " dims

#define GVA_VAAPI_TENSOR_CAPS_MAKE(types, dims)                                                                        \
    "" GVA_VAAPI_TENSORS_CAPS ","                                                                                      \
    "types = " types ","                                                                                               \
    "dimensions = " dims
