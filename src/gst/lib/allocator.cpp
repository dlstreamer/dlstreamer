/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/allocator.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/utils.h"

#include <gst/allocators/gstdmabuf.h>

using namespace dlstreamer;

#if defined(__clang__) || defined(__llvm__) // register GType under different name if build by SYCL

typedef GstAllocator GstDLStreamerAllocatorSYCL;
typedef GstAllocatorClass GstDLStreamerAllocatorSYCLClass;
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDLStreamerAllocatorSYCL, gst_object_unref)
G_DEFINE_TYPE(GstDLStreamerAllocatorSYCL, gst_dlstreamer_allocator, GST_TYPE_ALLOCATOR);

#else

typedef GstAllocator GstDLStreamerAllocator;
typedef GstAllocatorClass GstDLStreamerAllocatorClass;
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDLStreamerAllocator, gst_object_unref)
G_DEFINE_TYPE(GstDLStreamerAllocator, gst_dlstreamer_allocator, GST_TYPE_ALLOCATOR);

#endif

static void gst_dlstreamer_mem_free(GstAllocator * /*allocator*/, GstMemory *gmem) {
    auto *mem = GST_DLSTREAMER_MEMORY_CAST(gmem);
    delete mem;
}

static gpointer gst_dlstreamer_mem_map(GstMemory *gmem, gsize /*maxsize*/, GstMapFlags flags) {
    auto *mem = GST_DLSTREAMER_MEMORY_CAST(gmem);

    if (flags & GST_MAP_NATIVE_HANDLE) {
        return reinterpret_cast<gpointer>(mem->tensor->handle());
    } else if (flags == (flags & (GST_MAP_READ | GST_MAP_WRITE))) {
        if (!mem->mapped_tensor)
            mem->mapped_tensor = mem->tensor.map(gst_map_flags_to_access_mode(flags));
        return mem->mapped_tensor->data();
    } else {
        GST_ERROR("Unsupported map flag 0x%x", flags);
        return nullptr;
    }
}

static void gst_dlstreamer_mem_unmap(GstMemory *gmem) {
    auto *mem = GST_DLSTREAMER_MEMORY_CAST(gmem);
    mem->mapped_tensor = nullptr;
}

static GstMemory *gst_dlstreamer_mem_share(GstMemory * /*gmem*/, gssize /*offset*/, gssize /*size*/) {
    return nullptr;
}

static void gst_dlstreamer_allocator_class_init(GstAllocatorClass *klass) {
    klass->alloc = nullptr;
    klass->free = gst_dlstreamer_mem_free;
}

static void gst_dlstreamer_allocator_init(GstAllocator *allocator) {
    allocator->mem_map = gst_dlstreamer_mem_map;
    allocator->mem_unmap = gst_dlstreamer_mem_unmap;
    allocator->mem_share = gst_dlstreamer_mem_share;

    GST_OBJECT_FLAG_SET(allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

namespace dlstreamer {

GstAllocator *gst_dlstreamer_allocator_new(MemoryType memory_type) {
    GstAllocator *allocator = static_cast<GstAllocator *>(g_object_new(gst_dlstreamer_allocator_get_type(), NULL));
    DLS_CHECK(allocator)
    allocator->mem_type = memory_type_to_string(memory_type);
    return allocator;
}

GstMemory *gst_dlstreamer_allocator_wrap_tensor(GstAllocator *allocator, const TensorPtr &tensor) {
    auto dls_mem = new GstDLStreamerMemory;
    dls_mem->tensor = tensor;
    GstMemory *mem = &dls_mem->mem;
    auto size = tensor->info().nbytes();
    gst_memory_init(mem, static_cast<GstMemoryFlags>(0), allocator, NULL, size, 0, 0, size);
    return mem;
}

gboolean gst_is_dlstreamer_memory(GstMemory *mem) {
    return G_TYPE_CHECK_INSTANCE_TYPE(mem->allocator, gst_dlstreamer_allocator_get_type());
}

TensorPtr gst_dlstreamer_memory_get_tensor_ptr(GstMemory *mem) {
    assert(gst_is_dlstreamer_memory(mem));
    auto *dlmem = GST_DLSTREAMER_MEMORY_CAST(mem);
    return dlmem->tensor;
}

} // namespace dlstreamer
