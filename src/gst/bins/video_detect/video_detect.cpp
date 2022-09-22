/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_detect.h"
#include <sstream>

GST_DEBUG_CATEGORY(video_detect_debug_category);
#define GST_CAT_DEFAULT video_detect_debug_category

typedef struct _VideoDetect {
    VideoInference base;
} VideoDetect;

typedef struct _VideoDetectClass {
    VideoInferenceClass base;
} VideoDetectClass;

G_DEFINE_TYPE_WITH_CODE(VideoDetect, video_detect, GST_TYPE_VIDEO_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(video_detect_debug_category, "video_detect", 0,
                                                "debug category for video_detect element"));

static void video_detect_init(VideoDetect *self) {
    // default attach-tensor-data=false
    g_object_set(G_OBJECT(self), "attach-tensor-data", false, nullptr);
}

static void video_detect_class_init(VideoDetectClass *klass) {
    GST_DEBUG_CATEGORY_INIT(video_detect_debug_category, "video_detect", 0, "Debug category of gvadetect");

    auto video_inference = VIDEO_INFERENCE_CLASS(klass);
    video_inference->get_default_postprocess_elements = [](VideoInference * /*self*/) -> std::string {
        return "tensor_postproc_detection_output";
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_metadata(element_class, VIDEO_DETECT_NAME, "video", VIDEO_DETECT_DESCRIPTION,
                                   "Intel Corporation");
}
