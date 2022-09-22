/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_inference_openvino.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &tensor_inference_openvino, nullptr};
}
