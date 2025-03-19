/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_detect.h"
#include <cstdint>
#include <sstream>

GST_DEBUG_CATEGORY_STATIC(object_detect_debug_category);
#define GST_CAT_DEFAULT object_detect_debug_category
#define OBJECT_DETECT_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define OBJECT_DETECT_DESCRIPTION "Performs inference-based object detection"

struct ObjectDetect {
    VideoInference base;
};

struct ObjectDetectClass {
    VideoInferenceClass base;
};

G_DEFINE_TYPE_WITH_CODE(ObjectDetect, object_detect, GST_TYPE_VIDEO_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(object_detect_debug_category, "object_detect", 0,
                                                "debug category for object_detect element"));

static void object_detect_init(ObjectDetect *self) {
    // default attach-tensor-data=false
    g_object_set(G_OBJECT(self), "attach-tensor-data", (gboolean)FALSE, nullptr);
}

static void object_detect_class_init(ObjectDetectClass *klass) {
    GST_DEBUG_CATEGORY_INIT(object_detect_debug_category, "object_detect", 0, "Debug category of object_detect");

    auto video_inference = VIDEO_INFERENCE_CLASS(klass);
    video_inference->get_default_postprocess_elements = [](VideoInference *self) -> std::string {
        const char *model = self->get_model();
        if (!model)
            model = "";
        const char *p;
        if ((p = strstr(model, "yolo")) != nullptr) {
            uint8_t yolo_version = 0;
            if (!strncmp(p, "yolov", 5))
                yolo_version = p[5] - '0';
            else if (!strncmp(p, "yolo_v", 6) || !strncmp(p, "yolo-v", 6))
                yolo_version = p[6] - '0';
            return "tensor_postproc_yolo version=" + std::to_string(yolo_version);
        } else if (strstr(model, "human-pose")) {
            return "tensor_postproc_human_pose";
        } else {
            return "tensor_postproc_detection";
        }
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_metadata(element_class, OBJECT_DETECT_NAME, "video", OBJECT_DETECT_DESCRIPTION,
                                   "Intel Corporation");
}
