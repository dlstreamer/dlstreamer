/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_mapper.h"
#include <gst/gst.h>

namespace dlstreamer {

GST_EXPORT GstBuffer *buffer_to_gst_buffer(dlstreamer::BufferPtr buffer, dlstreamer::BufferMapperPtr cpu_mapper,
                                           std::string native_handle_id = {});

constexpr static GstMapFlags GST_MAP_NATIVE_HANDLE = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);
constexpr static GstMapFlags GST_MAP_DLS_BUFFER = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 2);

} // namespace dlstreamer
