/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "allocator_mock/allocator_mock.h"
#include "allocator_mock/gst_allocator_mock.h"

#include <gst_allocator_wrapper.h>

#include "gmock/gmock.h"
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

using namespace std;
using namespace InferenceBackend;

// GstAllocatorMock requires global object type of AllocatorMock that named allocator_mock
std::unique_ptr<AllocatorMock> allocator_mock;

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

struct GstAllocatorWrapperTest : public ::testing::Test {
    static GstAllocator *allocator;
    static constexpr const char *const allocator_name = "allocator_mock";

    unique_ptr<GstMemory> memory;

    const gsize size = 64;
    void *source_buffer = static_cast<void *>(0x2ff);

    // TODO: rename to SetUpTestSuite when migrate to googletest version higher than 1.8
    // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
    static void SetUpTestCase() {
        allocator = gst_allocator_mock_new();

        gst_allocator_register(allocator_name, allocator);
    }

    void SetUp() {
        allocator_mock = unique_ptr<AllocatorMock>(new AllocatorMock());
        memory = unique_ptr<GstMemory>(new GstMemory);
        memory->maxsize = size * 4;
        memory->allocator = allocator;
        memory->size = size;
        memory->align = 7;
        memory->offset = 0;

        GST_MINI_OBJECT_CAST(memory.get())->flags = GST_MINI_OBJECT_FLAG_LOCKABLE;
    }

    void TearDown() {
        allocator_mock.reset();
    }
};

GstAllocator *GstAllocatorWrapperTest::allocator = nullptr;

TEST_F(GstAllocatorWrapperTest, Initialization_test) {
    ASSERT_NO_THROW(unique_ptr<GstAllocatorWrapper>(new GstAllocatorWrapper(allocator_name)));
}

TEST_F(GstAllocatorWrapperTest, Alloc_test) {
    Allocator::AllocContext *alloc_context = nullptr;
    unique_ptr<GstAllocatorWrapper> wrapper(new GstAllocatorWrapper(allocator_name));

    EXPECT_CALL(*allocator_mock, gst_allocator_mock_alloc(allocator, size, _)).WillOnce(Return(memory.get()));
    EXPECT_CALL(*allocator_mock, gst_allocator_mock_map(memory.get(), memory->maxsize, GST_MAP_WRITE))
        .WillOnce(Return(source_buffer));

    void *buffer = nullptr;
    ASSERT_NO_THROW(wrapper->Alloc(size, buffer, alloc_context));

    ASSERT_EQ(source_buffer, buffer);
}

TEST_F(GstAllocatorWrapperTest, Free_test) {
    Allocator::AllocContext *alloc_context = nullptr;
    unique_ptr<GstAllocatorWrapper> wrapper(new GstAllocatorWrapper(allocator_name));

    EXPECT_CALL(*allocator_mock, gst_allocator_mock_alloc(allocator, _, _)).WillOnce(Return(memory.get()));
    EXPECT_CALL(*allocator_mock, gst_allocator_mock_map(memory.get(), _, _)).WillOnce(Return(source_buffer));

    void *buffer = nullptr;
    EXPECT_NO_THROW(wrapper->Alloc(size, buffer, alloc_context));

    EXPECT_CALL(*allocator_mock, gst_allocator_mock_unmap(memory.get()));
    EXPECT_CALL(*allocator_mock, gst_allocator_mock_free(allocator, memory.get()));
    ASSERT_NO_THROW(wrapper->Free(alloc_context));
}

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running GstAllocatorWrapperTest from " << __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    gst_check_init(&argc, &argv);
    return RUN_ALL_TESTS();
}
