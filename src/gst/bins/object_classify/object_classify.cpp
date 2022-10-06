/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_classify.h"
#include <algorithm>
#include <limits>

GST_DEBUG_CATEGORY(object_classify_debug_category);
#define GST_CAT_DEFAULT object_classify_debug_category
#define OBJECT_CLASSIFY_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define OBJECT_CLASSIFY_DESCRIPTION                                                                                    \
    "Performs object classification. Accepts the ROI or full frame as an input and "                                   \
    "outputs classification results with metadata."

struct ObjectClassify {
    VideoInference base;
};

struct ObjectClassifyClass {
    VideoInferenceClass base;
};

G_DEFINE_TYPE(ObjectClassify, object_classify, GST_TYPE_VIDEO_INFERENCE);

static void object_classify_init(ObjectClassify *self) {
    // default inference-region=roi-list
    g_object_set(G_OBJECT(self), "inference-region", Region::ROI_LIST, nullptr);
}

static void object_classify_class_init(ObjectClassifyClass *klass) {
    GST_DEBUG_CATEGORY_INIT(object_classify_debug_category, "object_classify", 0, "Debug category of object_classify");

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_metadata(element_class, OBJECT_CLASSIFY_NAME, "video", OBJECT_CLASSIFY_DESCRIPTION,
                                   "Intel Corporation");
}
