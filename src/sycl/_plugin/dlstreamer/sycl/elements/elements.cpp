/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "sycl_meta_overlay.h"
#include "sycl_tensor_histogram.h"

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &sycl_tensor_histogram, &sycl_meta_overlay,
    //
    nullptr};
}
