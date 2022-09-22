/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_classify.h"
#include <algorithm>
#include <limits>

GST_DEBUG_CATEGORY(video_classify_debug_category);
#define GST_CAT_DEFAULT video_classify_debug_category

typedef struct _VideoClassify {
    VideoInference base;
} VideoClassify;

typedef struct _VideoClassifyClass {
    VideoInferenceClass base;
} VideoClassifyClass;

G_DEFINE_TYPE(VideoClassify, video_classify, GST_TYPE_VIDEO_INFERENCE);

static void video_classify_init(VideoClassify *self) {
    // default inference-region=roi-list
    g_object_set(G_OBJECT(self), "inference-region", Region::ROI_LIST, nullptr);
}

static void video_classify_class_init(VideoClassifyClass *klass) {
    GST_DEBUG_CATEGORY_INIT(video_classify_debug_category, "video_classify", 0, "Debug category of video_classify");

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_metadata(element_class, VIDEO_CLASSIFY_NAME, "video", VIDEO_CLASSIFY_DESCRIPTION,
                                   "Intel Corporation");
}
