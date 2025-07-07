/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_frame.h"

#include <glib/gslice.h>
#include <gmock/gmock.h>
#include <gst/gstbuffer.h>
#include <gst/gstinfo.h>
#include <gst/gstmeta.h>
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

#include <gst/video/gstvideometa.h>

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

struct RegionOfInterestTest : public ::testing::Test {

    // TODO: rename to SetUpTestSuite when migrate to googletest version higher than 1.8
    // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
    // static void SetUpTestCase() {
    // }

    GstBuffer *buffer;
    GVA::RegionOfInterest *region_of_interest;

    void SetUp() {
        buffer = gst_buffer_new_and_alloc(0);
        GstVideoInfo info;
        gst_video_info_set_format(&info, GST_VIDEO_FORMAT_NV12, 1920, 1080); // FullHD
        GstVideoRegionOfInterestMeta *meta =
            gst_buffer_add_video_region_of_interest_meta(buffer, "detection", 0.0, 0.0, 0.3, 0.6);
        region_of_interest = new GVA::RegionOfInterest(meta);
    }

    void TearDown() {
        if (buffer)
            gst_buffer_unref(buffer);
        if (region_of_interest)
            delete region_of_interest;
    }
};

TEST_F(RegionOfInterestTest, RegionOfInterestTestTensors) {
    ASSERT_DOUBLE_EQ(region_of_interest->confidence(), 0.0);
    ASSERT_EQ(region_of_interest->tensors().size(), 0);

    std::vector<GVA::Tensor> test_tensors;
    const size_t tensors_num = 10;
    for (size_t i = 0; i < tensors_num; ++i) {
        GVA::Tensor tensor = region_of_interest->add_tensor("tensor_" + std::to_string(i));
        tensor.set_double("confidence", (double)i / (double)tensors_num);
        test_tensors.push_back(tensor);
    }
    GVA::Tensor detection = region_of_interest->add_tensor("detection");
    detection.set_double("confidence", 0.77);

    ASSERT_EQ(region_of_interest->tensors().size(), tensors_num + 1);
    ASSERT_DOUBLE_EQ(region_of_interest->confidence(), 0.77);
    ASSERT_DOUBLE_EQ((*region_of_interest).tensors()[5].confidence(), 0.5);

    double confidence = 0.0;
    for (GVA::Tensor tensor : region_of_interest->tensors()) {
        if (!tensor.is_detection()) {
            ASSERT_DOUBLE_EQ(tensor.confidence(), confidence);
            confidence += 0.1;
        } else
            ASSERT_DOUBLE_EQ(tensor.confidence(), 0.77);
    }

    ASSERT_EQ(region_of_interest->tensors().size(), test_tensors.size() + 1);
    auto tensors_roi = region_of_interest->tensors();
    for (size_t i = 0; i < tensors_num; ++i) {
        ASSERT_EQ(tensors_roi[i].confidence(), test_tensors[i].confidence());
    }
}
