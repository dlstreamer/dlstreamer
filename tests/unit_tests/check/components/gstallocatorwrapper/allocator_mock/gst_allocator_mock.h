/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/allocators/allocators.h>
#include <gst/gstallocator.h>

G_BEGIN_DECLS

typedef struct _GstAllocatorMock GstAllocatorMock;
typedef struct _GstAllocatorMockClass GstAllocatorMockClass;

#define GST_TYPE_ALLOCATOR_MOCK (gst_allocator_mock_get_type())
#define GST_IS_ALLOCATOR_MOCK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ALLOCATOR_MOCK))

#define GST_ALLOCATOR_MOCK_CAST(allocator) ((GstAllocatorMock *)(allocator))

struct _GstAllocatorMock {
    GstDmaBufAllocator parent;
};

GType gst_allocator_mock_get_type(void);
GstAllocator *gst_allocator_mock_new(void);

/**
 * GstAllocatorMockClass:
 *
 *
 */
struct _GstAllocatorMockClass {
    GstDmaBufAllocatorClass parent_class;
};

G_END_DECLS
