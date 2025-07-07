/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "mtcnn_common.h"
#include "test_common.h"
#include "test_utils.h"
#include "video_frame.h"

#include <gst/video/video.h>

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

typedef struct _GVADetection {
    gfloat x_min;
    gfloat y_min;
    gfloat x_max;
    gfloat y_max;
    gdouble confidence;
    gint label_id;
    gint object_id;
} GVADetection;

struct TestData {
    Resolution resolution;
    std::vector<GVADetection> boxes;
};

void setup_inbuffer(GstBuffer *inbuffer, gpointer user_data) {
    TestData *test_data = static_cast<TestData *>(user_data);
    ck_assert_msg(test_data != NULL, "Passed data is not TestData");

    GstVideoInfo info;
    gst_video_info_set_format(&info, TEST_BUFFER_VIDEO_FORMAT, test_data->resolution.width,
                              test_data->resolution.height);
    gst_buffer_add_video_meta(inbuffer, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_INFO_FORMAT(&info),
                              GST_VIDEO_INFO_WIDTH(&info), GST_VIDEO_INFO_HEIGHT(&info));
    GVA::VideoFrame video_frame(inbuffer, &info);

    for (auto input_bbox : test_data->boxes) {
        GVA::RegionOfInterest roi = video_frame.add_region(
            input_bbox.x_min * test_data->resolution.width, input_bbox.y_min * test_data->resolution.height,
            (input_bbox.x_max - input_bbox.x_min) * test_data->resolution.width,
            (input_bbox.y_max - input_bbox.y_min) * test_data->resolution.height);
        roi.add_tensor("bboxregression").set_double("score", input_bbox.confidence);
    }
}

void check_outbuffer(GstBuffer *outbuffer, gpointer user_data) {
    TestData *test_data = static_cast<TestData *>(user_data);
    ck_assert_msg(test_data != NULL, "Passed data is not TestData");

    GVA::VideoFrame video_frame(outbuffer);
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    ck_assert_msg(regions.size() == test_data->boxes.size(), "Some candidates have been merged");

    bool nms_tensor_exist = false;
    for (const GVA::RegionOfInterest &roi : regions) {
        for (const GVA::Tensor &tensor : roi.tensors()) {
            if (tensor.name() == "nms") {
                nms_tensor_exist = true;
                ck_assert_msg(tensor.has_field("score"), "No field \"score\" in structure");
                ck_assert_msg(tensor.get_double("score") > 0, "Invalid candiate has been included");
            }
        }
    }
    ck_assert_msg(nms_tensor_exist, "No structure with necessary name");
}

TestData test_data[] = {{{640, 480},
                         {{0.29375, 0.54375, 0.40625, 0.94167, 0.8, 0, 0},
                          {0.6078125, 0.59167, 0.7234375, 0.914583, 0.8, 1, 1},
                          {0.1172, 0.5417, 0.2391, 1, 0.8, 2, 2}}}};

GST_START_TEST(test_nms_rnet) {
    g_print("Starting test: test_nms_rnet\n");
    std::vector<std::string> supported_fp = {"FP32"};

    for (int i = 0; i < G_N_ELEMENTS(test_data); i++) {
        for (const auto &fp : supported_fp) {
            run_test("gvanms", VIDEO_CAPS_TEMPLATE_STRING, test_data[i].resolution, &srctemplate, &sinktemplate,
                     setup_inbuffer, check_outbuffer, &test_data[i], "threshold", 70, "mode", MODE_RNET, NULL);
        }
    }
}

GST_END_TEST;

static Suite *nms_suite(void) {
    Suite *s = suite_create("nms");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_nms_rnet);

    return s;
}

GST_CHECK_MAIN(nms);
