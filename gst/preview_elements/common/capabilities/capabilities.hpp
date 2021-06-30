/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

// TODO: define default values

#define GVA_TENSOR_CAPS_0                                                                                              \
    "application/tensor, "                                                                                             \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NC }, "                                                                                                \
    "batch-size = (int)[ 1, max ], "                                                                                   \
    "channels = (int)[ 0, max ];"

#define GVA_TENSOR_CAPS_1                                                                                              \
    "application/tensor, "                                                                                             \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { CHW }, "                                                                                               \
    "channels = (int)[ 0, max ], "                                                                                     \
    "dimension1 = (int)[ 0, max ], "                                                                                   \
    "dimension2 = (int)[ 0, max ];"

#define GVA_TENSOR_CAPS_2                                                                                              \
    "application/tensor, "                                                                                             \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NHWC, NCHW }, "                                                                                        \
    "batch-size = (int)[ 1, max ], "                                                                                   \
    "channels = (int)[ 0, max ], "                                                                                     \
    "dimension1 = (int)[ 0, max ], "                                                                                   \
    "dimension2 = (int)[ 0, max ];"

// TODO: unify caps with features
#define GVA_VAAPI_TENSOR_CAPS_0                                                                                        \
    "application/tensor(memory:VASurface), "                                                                           \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NC }, "                                                                                                \
    "batch-size = (int)[ 1, max ], "                                                                                   \
    "channels = (int)[ 0, max ];"

#define GVA_VAAPI_TENSOR_CAPS_1                                                                                        \
    "application/tensor(memory:VASurface), "                                                                           \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { CHW }, "                                                                                               \
    "channels = (int)[ 0, max ], "                                                                                     \
    "dimension1 = (int)[ 0, max ], "                                                                                   \
    "dimension2 = (int)[ 0, max ];"

#define GVA_VAAPI_TENSOR_CAPS_2                                                                                        \
    "application/tensor(memory:VASurface), "                                                                           \
    "precision = { FP32, FP16, U8 }, "                                                                                 \
    "layout = { NHWC, NCHW }, "                                                                                        \
    "batch-size = (int)[ 1, max ], "                                                                                   \
    "channels = (int)[ 0, max ], "                                                                                     \
    "dimension1 = (int)[ 0, max ], "                                                                                   \
    "dimension2 = (int)[ 0, max ];"

#define GVA_TENSOR_CAPS GVA_TENSOR_CAPS_0 GVA_TENSOR_CAPS_1 GVA_TENSOR_CAPS_2
#define GVA_VAAPI_TENSOR_CAPS GVA_VAAPI_TENSOR_CAPS_0 GVA_VAAPI_TENSOR_CAPS_1 GVA_VAAPI_TENSOR_CAPS_2
