/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include <gst/gst.h>

namespace dlstreamer {

constexpr static GstMapFlags GST_MAP_NATIVE_HANDLE = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);

G_BEGIN_DECLS

GstAllocator *gst_dlstreamer_allocator_new(MemoryType memory_type);

GstMemory *gst_dlstreamer_allocator_wrap_tensor(GstAllocator *allocator, const TensorPtr &tensor);

gboolean gst_is_dlstreamer_memory(GstMemory *mem);

// direct access to struct GstDLStreamerMemory not recommended, use API functions instead
struct GstDLStreamerMemory {
    GstMemory mem;
    TensorPtr tensor;
    TensorPtr mapped_tensor;
};

#define GST_DLSTREAMER_MEMORY_CAST(mem) ((GstDLStreamerMemory *)(mem))

#define GST_DLSTREAMER_ALLOCATOR_TYPE_NAME "GstDLStreamerAllocator"

G_END_DECLS

// function returns TensorPtr and available only for C++
#ifdef __cplusplus
TensorPtr gst_dlstreamer_memory_get_tensor_ptr(GstMemory *mem);
#endif

} // namespace dlstreamer
