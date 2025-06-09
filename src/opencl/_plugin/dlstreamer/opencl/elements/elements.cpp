/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencl_tensor_normalize.h"
#ifdef DLS_HAVE_VAAPI
#include "vaapi_to_opencl.h"
#endif

extern "C" {

#if !(_MSC_VER)
DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
#else
const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
#endif
    &opencl_tensor_normalize,
#ifdef DLS_HAVE_VAAPI
    &vaapi_to_opencl,
#endif
    nullptr};
}
