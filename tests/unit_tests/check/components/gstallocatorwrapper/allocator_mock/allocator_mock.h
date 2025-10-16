/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gmock/gmock.h>
#include <gst/allocators/allocators.h>
#include <gst/gstallocator.h>

struct _GstAllocatorMockClass;
struct _GstAllocatorMock;

class IAllocatorMock {
  public:
    virtual ~IAllocatorMock() = default;
    virtual void gst_allocator_mock_free(GstAllocator *, GstMemory *) = 0;
    virtual GstMemory *gst_allocator_mock_alloc(GstAllocator *, gsize, GstAllocationParams *) = 0;
    virtual gpointer gst_allocator_mock_map(GstMemory *, gsize, GstMapFlags) = 0;
    virtual void gst_allocator_mock_unmap(GstMemory *) = 0;
    virtual void gst_allocator_mock_class_init(_GstAllocatorMockClass *) = 0;
    virtual void gst_allocator_mock_init(_GstAllocatorMock *) = 0;
    virtual GstAllocator *gst_allocator_mock_new(void) = 0;
};

class AllocatorMock : public IAllocatorMock {
  public:
    AllocatorMock() = default;
    ~AllocatorMock() = default;
    // MOCK_METHOD1(gst_allocator_mock_finalize, void(GObject*));

    MOCK_METHOD2(gst_allocator_mock_free, void(GstAllocator *, GstMemory *));
    MOCK_METHOD3(gst_allocator_mock_alloc, GstMemory *(GstAllocator *, gsize, GstAllocationParams *));

    MOCK_METHOD3(gst_allocator_mock_map, gpointer(GstMemory *, gsize, GstMapFlags));
    MOCK_METHOD1(gst_allocator_mock_unmap, void(GstMemory *));

    MOCK_METHOD1(gst_allocator_mock_class_init, void(_GstAllocatorMockClass *));
    MOCK_METHOD1(gst_allocator_mock_init, void(_GstAllocatorMock *));
    MOCK_METHOD0(gst_allocator_mock_new, GstAllocator *(void));
};
