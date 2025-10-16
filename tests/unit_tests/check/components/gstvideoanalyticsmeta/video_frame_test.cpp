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

struct VideoFrameTest : public ::testing::Test {

    // TODO: rename to SetUpTestSuite when migrate to googletest version higher than 1.8
    // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
    // static void SetUpTestCase() {
    // }

    friend class GVA::VideoFrame;
    GstBuffer *buffer;
    GVA::VideoFrame *frame;

    void SetUp() {
        buffer = gst_buffer_new_and_alloc(0);
        GstVideoInfo info;
        gst_video_info_set_format(&info, GST_VIDEO_FORMAT_NV12, 1920, 1080); // FullHD
        frame = new GVA::VideoFrame(buffer, &info);
    }

    void TearDown() {
        if (buffer)
            gst_buffer_unref(buffer);
        if (frame)
            delete frame;
    }
};

TEST_F(VideoFrameTest, VideoFrameTestRegions) {
    ASSERT_EQ(frame->regions().size(), 0);
    ASSERT_EQ(frame->tensors().size(), 0);

    std::vector<std::string> labels{"Face", "Person", "Vehicle"};
    constexpr int ROIS_NUMBER = 100;
    for (int i = 0; i < ROIS_NUMBER; ++i) {
        frame->add_region(i, i, i + 100, i + 100, labels[i % 3], i / 100.0);
    }
    std::vector<GVA::RegionOfInterest> regions = frame->regions();

    for (int i = 0; i < ROIS_NUMBER; i++) {
        ASSERT_EQ(regions[i].label(), labels[i % 3]);
    }
    ASSERT_EQ(frame->regions().size(), ROIS_NUMBER);

    // unsigned idx = 1; // start with 1 because 0 region was popped
    unsigned idx = 0;
    regions = frame->regions();
    for (GVA::RegionOfInterest &roi : frame->regions()) {
        std::vector<GVA::RegionOfInterest>::iterator pos_elem_reg =
            std::find_if(regions.begin(), regions.end(), [idx](GVA::RegionOfInterest roi) {
                auto rect = roi.rect();
                return (rect.x == idx and rect.y == idx and rect.h == idx + 100 and rect.w == idx + 100);
            });
        if (pos_elem_reg != regions.end())
            regions.erase(pos_elem_reg);
        ++idx;
    }
    ASSERT_EQ(regions.size(), 0);

    // ROI in clipped coordinates can be added if it is bounded to [0,1]
    GVA::RegionOfInterest roi1 = frame->add_region(0.0, 0.0, 0.3, 0.6, "label", 0.8, true);
    ASSERT_EQ(frame->regions().size(), ROIS_NUMBER + 1);

    // ROI will be clipped (before adding) if it is not bounded to [0,1]
    GVA::RegionOfInterest roi1_bad = frame->add_region(0.7, 0.3, 0.35, 0.1, "label", 0.8, true);

    // name must change "roi1_bad" -> "detection", because we're adding region, hence it Tensor must be named
    // "detection"
    GstStructure *detection_param = gst_video_region_of_interest_meta_get_param(roi1_bad._meta(), "detection");
    double x_max = 0;
    gst_structure_get_double(detection_param, "x_max", &x_max);
    ASSERT_FLOAT_EQ(x_max, 1.0); // 0.7 {x} + 0.35 {w} = 1.05 {x_max}, which is more than 1.0, so width will be clipped
                                 // to 0.3 from 0.35, so that 0.7 {x} + 0.3 {w} = 1.0 {x_max}
    ASSERT_EQ(frame->regions().size(), ROIS_NUMBER + 2);

    // ROI in abs coordinates can be added if it is bounded to FullHD
    GVA::RegionOfInterest roi2 = frame->add_region(0, 0, 1000, 1000, "label", 0.8);
    ASSERT_EQ(frame->regions().size(), ROIS_NUMBER + 3);

    // ROI will be clipped (before adding) if it is not bounded to FullHD
    GVA::RegionOfInterest roi2_bad = frame->add_region(1900, 1000, 100, 100, "label", 0.8);
    ASSERT_EQ(roi2_bad._meta()->w, 20); // 1900 {x} + 100 {w} = 2000, which is more than 1920, so width will be clipped
                                        // to 20 from 100, so that 1900 {x} + 20 {w} = 1920
    ASSERT_EQ(roi2_bad._meta()->h, 80); // 1000 {y} + 100 {w} = 1100, which is more than 1080, so width will be clipped
                                        // to 80 from 100, so that 1000 {x} + 80 {w} = 1080
    ASSERT_EQ(frame->regions().size(), ROIS_NUMBER + 4);

    // check intitial roi2_bad
    ASSERT_EQ(roi2_bad._meta()->x, 1900);
    ASSERT_EQ(roi2_bad._meta()->y, 1000);
    ASSERT_EQ(roi2_bad._meta()->h, 80);
    ASSERT_EQ(roi2_bad._meta()->w, 20);

    auto rect = roi2_bad.rect();
    ASSERT_EQ(rect.x, 1900);
    ASSERT_EQ(rect.y, 1000);
    ASSERT_EQ(rect.h, 80);
    ASSERT_EQ(rect.w, 20);

    // no changes for tensor meta
    ASSERT_EQ(frame->tensors().size(), 0);
}

TEST_F(VideoFrameTest, VideoFrameTestTensors) {
    ASSERT_EQ(frame->tensors().size(), 0);

    const size_t tensor_meta_size = 10;
    const std::string field_name = "model_name";
    const std::string model_name = "test_model";
    for (size_t i = 0; i < tensor_meta_size; ++i) {
        GVA::Tensor tensor = frame->add_tensor();
        std::string test_model = model_name + std::to_string(i);
        gst_structure_set(tensor.gst_structure(), field_name.data(), G_TYPE_STRING, test_model.data(), NULL);
    }

    std::vector<GVA::Tensor> frame_tensors = frame->tensors();
    ASSERT_EQ(frame_tensors.size(), tensor_meta_size);

    unsigned idx = 0;
    for (GVA::Tensor &tensor : frame->tensors()) {
        std::string test_model_name = model_name + std::to_string(idx);
        std::vector<GVA::Tensor>::iterator pos_elem =
            std::find_if(frame_tensors.begin(), frame_tensors.end(), [test_model_name, field_name](GVA::Tensor tensor) {
                return (tensor.get_string(field_name) == test_model_name);
            });
        if (pos_elem != frame_tensors.end())
            frame_tensors.erase(pos_elem);
        ++idx;
    }
    ASSERT_EQ(frame_tensors.size(), 0);
}

TEST_F(VideoFrameTest, VideoFrameTestJSONMessages) {
    ASSERT_EQ(frame->messages().size(), 0);

    constexpr int MESSAGES_NUMBER = 10;
    std::vector<std::string> test_messages;
    for (int i = 0; i < MESSAGES_NUMBER; ++i) {
        std::string test_message = "test_message_" + std::to_string(i);
        test_messages.push_back(test_message);
        frame->add_message(test_message);
    }

    ASSERT_EQ(frame->messages().size(), MESSAGES_NUMBER);
    std::vector<std::string> messages = frame->messages();

    for (auto it = messages.begin(); it != messages.end(); ++it) {
        auto pos_mes = std::find(test_messages.begin(), test_messages.end(), *it);
        if (pos_mes != test_messages.end())
            test_messages.erase(pos_mes);
    }
    ASSERT_EQ(test_messages.size(), 0);

    GstGVAJSONMeta *meta = NULL;
    gpointer state = NULL;
    unsigned index = 0;
    while ((meta = GST_GVA_JSON_META_ITERATE(buffer, &state))) {
        meta->message = strcat(strdup(std::to_string(index + 10).c_str()), "test_message");
        ++index;
    }

    for (int i = 0; i < MESSAGES_NUMBER; ++i) {
        std::string test_message = std::to_string(i + MESSAGES_NUMBER) + "test_message";
        test_messages.push_back(test_message);
    }

    messages = frame->messages();
    ASSERT_EQ(messages.size(), MESSAGES_NUMBER);

    for (auto it = messages.begin(); it != messages.end(); ++it) {
        auto pos_mes = std::find(test_messages.begin(), test_messages.end(), *it);
        if (pos_mes != test_messages.end())
            test_messages.erase(pos_mes);
    }
    ASSERT_EQ(test_messages.size(), 0);
}

TEST_F(VideoFrameTest, VideoFrameTestRegionIDs) {
    ASSERT_EQ(frame->regions().size(), 0);
    ASSERT_EQ(frame->tensors().size(), 0);

    std::vector<std::string> labels{"Face", "Person", "Vehicle"};
    constexpr int ROIS_NUMBER = 100;
    for (int i = 0; i < ROIS_NUMBER; ++i) {
        frame->add_region(i, i, i + 100, i + 100, labels[i % 3], i / 100.0);
    }
    std::vector<GVA::RegionOfInterest> regions = frame->regions();
    ASSERT_EQ(regions.size(), ROIS_NUMBER);

    std::map<int, int> ids_entry;
    for (int i = 0; i < ROIS_NUMBER; i++) {
        int roi_id = regions[i].region_id();
        ASSERT_GT(roi_id, -1) << "Region ID should be non-negative";
        ids_entry[roi_id]++;
    }

    for (auto k : ids_entry) {
        ASSERT_EQ(k.second, 1) << "ID should be unique";
    }
}
