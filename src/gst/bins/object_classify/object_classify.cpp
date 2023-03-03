/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_classify.h"
#include <algorithm>
#include <limits>

GST_DEBUG_CATEGORY_STATIC(object_classify_debug_category);
#define GST_CAT_DEFAULT object_classify_debug_category
#define OBJECT_CLASSIFY_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define OBJECT_CLASSIFY_DESCRIPTION                                                                                    \
    "Performs object classification. Accepts the ROI or full frame as an input and "                                   \
    "outputs classification results with metadata."

enum {
    PROP_0,
    PROP_RECLASSIFY_INTERVAL,
};

#define DEFAULT_RECLASSIFY_INTERVAL 1
#define DEFAULT_MIN_RECLASSIFY_INTERVAL 0
#define DEFAULT_MAX_RECLASSIFY_INTERVAL UINT_MAX

struct ObjectClassify {
    VideoInference base;
    guint reclassify_interval;
};

struct ObjectClassifyClass {
    VideoInferenceClass base;
};

G_DEFINE_TYPE(ObjectClassify, object_classify, GST_TYPE_VIDEO_INFERENCE);

#define OBJECT_CLASSIFY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), object_classify_get_type(), ObjectClassify))

static void object_classify_init(ObjectClassify *self) {
    self->reclassify_interval = DEFAULT_RECLASSIFY_INTERVAL;

    // default inference-region=roi-list
    g_object_set(G_OBJECT(self), "inference-region", Region::ROI_LIST, nullptr);
}

void object_classify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    ObjectClassify *self = OBJECT_CLASSIFY(object);
    switch (property_id) {
    case PROP_RECLASSIFY_INTERVAL:
        g_value_set_uint(value, self->reclassify_interval);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void object_classify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    ObjectClassify *self = OBJECT_CLASSIFY(object);
    switch (property_id) {
    case PROP_RECLASSIFY_INTERVAL:
        self->reclassify_interval = g_value_get_uint(value);
        g_object_set(G_OBJECT(self), "roi-inference-interval", self->reclassify_interval, nullptr);
        g_object_set(G_OBJECT(self), "repeat-metadata", (gboolean)TRUE, nullptr);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void object_classify_class_init(ObjectClassifyClass *klass) {
    GST_DEBUG_CATEGORY_INIT(object_classify_debug_category, "object_classify", 0, "Debug category of object_classify");

    auto gobject_class = G_OBJECT_CLASS(klass);
    auto element_class = GST_ELEMENT_CLASS(klass);
    gobject_class->get_property = object_classify_get_property;
    gobject_class->set_property = object_classify_set_property;

    g_object_class_install_property(
        gobject_class, PROP_RECLASSIFY_INTERVAL,
        g_param_spec_uint(
            "reclassify-interval", "Reclassify Interval",
            "Determines how often to reclassify tracked objects. Only valid when used in conjunction with gvatrack.\n"
            "The following values are acceptable:\n"
            "- 0 - Do not reclassify tracked objects\n"
            "- 1 - Always reclassify tracked objects\n"
            "- 2:N - Tracked objects will be reclassified every N frames. Note the inference-interval is applied "
            "before "
            "determining if an object is to be reclassified (i.e. classification only occurs at a multiple of the "
            "inference interval)",
            DEFAULT_MIN_RECLASSIFY_INTERVAL, DEFAULT_MAX_RECLASSIFY_INTERVAL, DEFAULT_RECLASSIFY_INTERVAL,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_set_metadata(element_class, OBJECT_CLASSIFY_NAME, "video", OBJECT_CLASSIFY_DESCRIPTION,
                                   "Intel Corporation");
}
