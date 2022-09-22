/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_sync.h"
#include "video_preproc_vaapi.h"

extern "C" {
DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &vaapi_sync, &video_preproc_vaapi,
    //
    nullptr};
}
