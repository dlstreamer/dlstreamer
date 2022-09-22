/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_normalize_opencl.h"
#ifdef DLS_HAVE_VAAPI
#include "vaapi_to_opencl.h"
#endif

extern "C" {

DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &tensor_normalize_opencl,
#ifdef DLS_HAVE_VAAPI
    &vaapi_to_opencl,
#endif
    nullptr};
}
