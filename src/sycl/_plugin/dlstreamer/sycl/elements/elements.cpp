/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_histogram_sycl.h"
#include "watermark_sycl.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &tensor_histogram_sycl, &watermark_sycl,
    //
    nullptr};
}
