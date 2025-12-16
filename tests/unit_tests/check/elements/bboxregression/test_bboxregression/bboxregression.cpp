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
    float conv_buffer[4];
    float prob_buffer[2];
};

void add_param_with_score_to_roi_meta(GstVideoRegionOfInterestMeta *meta, double score) {
    GstStructure *s = gst_structure_new("nms", "score", G_TYPE_DOUBLE, score, NULL);
    gst_video_region_of_interest_meta_add_param(meta, s);
}

void add_param_with_buffer_to_roi_meta(GstVideoRegionOfInterestMeta *meta, std::string layer_name, void *buffer,
                                       int size) {
    GstStructure *s = gst_structure_new(("layer:" + layer_name).c_str(), "layer_name", G_TYPE_STRING, layer_name.data(),
                                        "model_name", G_TYPE_STRING, "RNet", "precision", G_TYPE_INT, 20, "layout",
                                        G_TYPE_INT, 193, "rank", G_TYPE_INT, 2, NULL);
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(s, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
    gst_video_region_of_interest_meta_add_param(meta, s);
}

void setup_inbuffer(GstBuffer *inbuffer, gpointer user_data) {
    TestData *test_data = static_cast<TestData *>(user_data);
    ck_assert_msg(test_data != NULL, "Passed data is not TestData");

    GstVideoInfo info;
    gst_video_info_set_format(&info, TEST_BUFFER_VIDEO_FORMAT, test_data->resolution.width,
                              test_data->resolution.height);
    gst_buffer_add_video_meta(inbuffer, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_INFO_FORMAT(&info),
                              GST_VIDEO_INFO_WIDTH(&info), GST_VIDEO_INFO_HEIGHT(&info));

    for (auto input_bbox : test_data->boxes) {
        GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
            inbuffer, NULL, input_bbox.x_min * test_data->resolution.width,
            input_bbox.y_min * test_data->resolution.height,
            (input_bbox.x_max - input_bbox.x_min) * test_data->resolution.width,
            (input_bbox.y_max - input_bbox.y_min) * test_data->resolution.height);

        add_param_with_score_to_roi_meta(meta, input_bbox.confidence);
        add_param_with_buffer_to_roi_meta(meta, "conv5-2", test_data->conv_buffer, sizeof(test_data->conv_buffer));
        add_param_with_buffer_to_roi_meta(meta, "prob1", test_data->prob_buffer, sizeof(test_data->prob_buffer));
    }
}

void check_outbuffer(GstBuffer *outbuffer, gpointer user_data) {
    TestData *test_data = static_cast<TestData *>(user_data);
    ck_assert_msg(test_data != NULL, "Passed data is not TestData");

    GVA::VideoFrame video_frame(outbuffer);
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();

    ck_assert_msg(regions.size() == test_data->boxes.size(), "Not all candidates have been generated");

    bool bboxregression_meta_exist = false;
    for (const GVA::RegionOfInterest &roi : regions) {
        for (const GVA::Tensor &tensor : roi.tensors()) {
            if (tensor.name() == "bboxregression") {
                bboxregression_meta_exist = true;
                ck_assert_msg(tensor.has_field("score"), "No field \"score\" in structure");
                ck_assert_msg(tensor.get_double("score") > 0, "Invalid candiate has been included");
            }
        }
    }

    ck_assert_msg(bboxregression_meta_exist, "No structure with necessary name");
}

TestData test_data[] = {{{640, 480},
                         {{0.29375, 0.54375, 0.40625, 0.94167, 0.8, 0, 0}},
                         {0.001355, -0.092506, -0.051913, 0.258041},
                         {0.124721, 0.875279}}};

GST_START_TEST(test_bboxregression_rnet) {
    g_print("Starting test: test_bboxregression_rnet\n");
    std::vector<std::string> supported_fp = {"FP32"};

    for (int i = 0; i < G_N_ELEMENTS(test_data); i++) {
        for (const auto &fp : supported_fp) {
            run_test("gvabboxregression", VIDEO_CAPS_TEMPLATE_STRING, test_data[i].resolution, &srctemplate,
                     &sinktemplate, setup_inbuffer, check_outbuffer, &test_data[i], "mode", MODE_RNET, NULL);
        }
    }
}

GST_END_TEST;

static Suite *bboxregression_suite(void) {
    Suite *s = suite_create("bboxregression");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_bboxregression_rnet);

    return s;
}

GST_CHECK_MAIN(bboxregression);
