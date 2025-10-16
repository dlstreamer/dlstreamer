/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gst_allocator_mock.h"
#include "allocator_mock.h"

// GstAllocatorMock requires global object type of AllocatorMock that named allocator_mock
extern std::unique_ptr<AllocatorMock> allocator_mock;

G_DEFINE_TYPE(GstAllocatorMock, gst_allocator_mock, GST_TYPE_ALLOCATOR);

static void gst_allocator_mock_finalize(GObject *object) {
    GstAllocatorMock *const allocator = GST_ALLOCATOR_MOCK_CAST(object);

    // FIXME: how does it work???
    // G_OBJECT_CLASS(gst_allocator_mock_parent_class)->finalize(object);
}

static void gst_allocator_mock_free(GstAllocator *allocator, GstMemory *memory) {
    allocator_mock->gst_allocator_mock_free(allocator, memory);
}

static GstMemory *gst_allocator_mock_alloc(GstAllocator *base_allocator, gsize size, GstAllocationParams *params) {
    return allocator_mock->gst_allocator_mock_alloc(base_allocator, size, params);
}

static gpointer gst_allocator_mock_map(GstMemory *memory, gsize size, GstMapFlags flags) {
    return allocator_mock->gst_allocator_mock_map(memory, size, flags);
}
static void gst_allocator_mock_unmap(GstMemory *memory) {
    allocator_mock->gst_allocator_mock_unmap(memory);
}

static void gst_allocator_mock_class_init(GstAllocatorMockClass *klass) {
    GObjectClass *const object_class = G_OBJECT_CLASS(klass);
    GstAllocatorClass *const allocator_class = GST_ALLOCATOR_CLASS(klass);

    object_class->finalize = gst_allocator_mock_finalize;
    allocator_class->alloc = gst_allocator_mock_alloc;
    allocator_class->free = gst_allocator_mock_free;
}

static void gst_allocator_mock_init(GstAllocatorMock *allocator) {
    GstAllocator *alloc = GST_ALLOCATOR_CAST(allocator);
    alloc->mem_map = gst_allocator_mock_map;
    alloc->mem_unmap = gst_allocator_mock_unmap;

    alloc->mem_map_full = NULL;
    alloc->mem_unmap_full = NULL;
}

GstAllocator *gst_allocator_mock_new(void) {
    GstAllocatorMock *allocator = NULL;
    allocator = reinterpret_cast<GstAllocatorMock *>(g_object_new(GST_TYPE_ALLOCATOR_MOCK, NULL));
    return GST_ALLOCATOR_CAST(allocator);
}
