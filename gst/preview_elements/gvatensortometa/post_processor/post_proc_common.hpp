/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <vector>

using MetasTable = std::vector<std::vector<GstStructure *>>;

struct ModelImageInputInfo {
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = -1;
    int memory_type = -1;
};
