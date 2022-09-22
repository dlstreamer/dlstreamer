/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "multi_source_ffmpeg.h"

extern "C" {
DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &multi_source_ffmpeg, nullptr};
}
