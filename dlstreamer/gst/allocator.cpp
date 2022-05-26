/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/allocator.h"
#include "dlstreamer/gst/buffer.h"
#include <gst/gst.h>

using namespace dlstreamer;

G_BEGIN_DECLS

struct GstDLSMemory {
    GstMemory mem;
    BufferPtr buffer;
    BufferMapperPtr mapper;
    BufferPtr mapped_buffer;
    std::string native_handle_id;
    int plane_index;

    static GstDLSMemory *unpack(GstMemory *mem) {
        return reinterpret_cast<GstDLSMemory *>(mem);
    }
};

struct GstDLSAllocator {
    GstAllocator parent;
};

struct GstDLSAllocatorClass {
    GstAllocatorClass parent_class;
};

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstDLSAllocator, gst_object_unref)

G_DEFINE_TYPE(GstDLSAllocator, gst_dls_allocator, GST_TYPE_ALLOCATOR);

G_END_DECLS

namespace dlstreamer {

GstBuffer *buffer_to_gst_buffer(dlstreamer::BufferPtr buffer, dlstreamer::BufferMapperPtr cpu_mapper,
                                std::string native_handle_id) {
    GstAllocator *allocator = static_cast<GstAllocator *>(g_object_new(gst_dls_allocator_get_type(), NULL));
    allocator->mem_type = buffer_type_to_string(buffer->type()).data();
    auto info = buffer->info();
    GstBuffer *buf = gst_buffer_new();
    for (size_t i = 0; i < info->planes.size(); i++) {
        GstDLSMemory *mem = new GstDLSMemory;
        auto size = info->planes[i].size();
        gst_memory_init(GST_MEMORY_CAST(mem), static_cast<GstMemoryFlags>(0), allocator, NULL, size, 0, 0, size);
        mem->plane_index = i;
        mem->buffer = buffer;
        mem->mapper = cpu_mapper;
        mem->native_handle_id = native_handle_id;
        gst_buffer_insert_memory(buf, i, &mem->mem);
    }
    gst_object_unref(allocator);

    // copy metadata
    GSTBuffer dst(buf, buffer->info());
    copy_metadata(*buffer, dst);

    return buf;
}

} // namespace dlstreamer

static void gst_dls_mem_free(GstAllocator * /*allocator*/, GstMemory *gmem) {
    auto *mem = GstDLSMemory::unpack(gmem);
    delete mem;
}

static gpointer gst_dls_mem_map(GstMemory *gmem, gsize /*maxsize*/, GstMapFlags flags) {
    auto *mem = GstDLSMemory::unpack(gmem);

    if (flags & GST_MAP_NATIVE_HANDLE) {
        std::string native_handle_id = mem->native_handle_id;
        if (native_handle_id.empty()) {
            native_handle_id = *mem->buffer->keys().begin();
        }
        return reinterpret_cast<gpointer>(mem->buffer->handle(native_handle_id, mem->plane_index));
    } else if (flags & GST_MAP_DLS_BUFFER) {
        return mem->buffer.get();
    } else if (mem->mapper) {
        int mode = 0;
        if (flags & GST_MAP_READ)
            mode |= static_cast<int>(AccessMode::READ);
        if (flags & GST_MAP_WRITE)
            mode |= static_cast<int>(AccessMode::WRITE);
        mem->mapped_buffer = mem->mapper->map(mem->buffer, static_cast<AccessMode>(mode));
        return mem->mapped_buffer->data(mem->plane_index);
    } else {
        return mem->buffer->data();
    }
}

static void gst_dls_mem_unmap(GstMemory *gmem) {
    auto *mem = GstDLSMemory::unpack(gmem);
    mem->mapped_buffer = nullptr;
}

static GstMemory *gst_dls_mem_share(GstMemory * /*gmem*/, gssize /*offset*/, gssize /*size*/) {
    return nullptr;
}

static void gst_dls_allocator_class_init(GstDLSAllocatorClass *klass) {
    GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS(klass);

    allocator_class->alloc = nullptr;
    allocator_class->free = gst_dls_mem_free;
}

static void gst_dls_allocator_init(GstDLSAllocator *allocator) {
    GstAllocator *alloc = GST_ALLOCATOR_CAST(allocator);

    alloc->mem_map = gst_dls_mem_map;
    alloc->mem_unmap = gst_dls_mem_unmap;
    alloc->mem_share = gst_dls_mem_share;

    GST_OBJECT_FLAG_SET(allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}
