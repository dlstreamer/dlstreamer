/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ffmpeg_multi_source.h"

extern "C" {
DLS_EXPORT const dlstreamer::ElementDesc *dlstreamer_elements[] = { //
    &ffmpeg_multi_source, nullptr};
}
