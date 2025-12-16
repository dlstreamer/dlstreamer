/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gstgvaclassify.h>
#include <gva_base_inference.h>

#include <test_utils.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

#include <gst/video/gstvideometa.h>

TEST(GvaClassifyTest, CheckPipeline) {
    GstGvaClassify *gva_classify =
        (GstGvaClassify *)g_object_new_with_properties(gst_gva_classify_get_type(), 0, NULL, NULL);
    GvaBaseInference *base_inference = GVA_BASE_INFERENCE(gva_classify);
}
