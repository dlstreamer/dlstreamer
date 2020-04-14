/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#include "classification_history.h"
#include "gstgvaclassify.h"
#include "gva_caps.h"
#include "post_processors.h"
#include "pre_processors.h"

#define ELEMENT_LONG_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

enum {
    PROP_0,
    PROP_OBJECT_CLASS,
    PROP_RECLASSIFY_INTERVAL,
};

#define DEFAULT_OBJECT_CLASS ""
#define DEFAULT_RECLASSIFY_INTERVAL 1
#define DEFAULT_MIN_RECLASSIFY_INTERVAL 0
#define DEFAULT_MAX_RECLASSIFY_INTERVAL UINT_MAX

GST_DEBUG_CATEGORY_STATIC(gst_gva_classify_debug_category);
#define GST_CAT_DEFAULT gst_gva_classify_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaClassify, gst_gva_classify, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_classify_debug_category, "gvaclassify", 0,
                                                "debug category for gvaclassify element"));

#define UNUSED(x) (void)(x)

static GstPadProbeReturn FillROIParamsCallback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
static void gst_gva_classify_finalize(GObject *);
static void gst_gva_classify_cleanup(GstGvaClassify *);

void gst_gva_classify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaClassify *gvaclassify = (GstGvaClassify *)(object);

    GST_DEBUG_OBJECT(gvaclassify, "set_property");

    static gulong probe_id = 0;

    switch (property_id) {
    case PROP_OBJECT_CLASS: {
        g_free(gvaclassify->object_class);
        gvaclassify->object_class = g_value_dup_string(value);
        break;
    }
    case PROP_RECLASSIFY_INTERVAL: {
        guint newValue = g_value_get_uint(value);
        guint oldValue = gvaclassify->reclassify_interval;
        if (newValue != oldValue) {
            if (oldValue == DEFAULT_RECLASSIFY_INTERVAL) {
                probe_id =
                    gst_pad_add_probe(gvaclassify->base_inference.base_transform.srcpad, GST_PAD_PROBE_TYPE_BUFFER,
                                      FillROIParamsCallback, gvaclassify->classification_history, NULL);
            } else if (newValue == DEFAULT_RECLASSIFY_INTERVAL) { // not tested
                gst_pad_remove_probe(gvaclassify->base_inference.base_transform.srcpad, probe_id);
                probe_id = 0;
            }
            gvaclassify->reclassify_interval = newValue;
        }
        break;
    }
    default: {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
    }
}

void gst_gva_classify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaClassify *gvaclassify = (GstGvaClassify *)(object);

    GST_DEBUG_OBJECT(gvaclassify, "get_property");

    switch (property_id) {
    case PROP_OBJECT_CLASS:
        g_value_set_string(value, gvaclassify->object_class);
        break;
    case PROP_RECLASSIFY_INTERVAL:
        g_value_set_uint(value, gvaclassify->reclassify_interval);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_classify_class_init(GstGvaClassifyClass *gvaclassify_class) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(gvaclassify_class);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    GObjectClass *gobject_class = G_OBJECT_CLASS(gvaclassify_class);
    gobject_class->set_property = gst_gva_classify_set_property;
    gobject_class->get_property = gst_gva_classify_get_property;
    gobject_class->finalize = gst_gva_classify_finalize;

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "ObjectClass",
                            "Specifies the Region of Interest type for which this classifier will run",
                            DEFAULT_OBJECT_CLASS, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
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
}

void gst_gva_classify_init(GstGvaClassify *gvaclassify) {
    GST_DEBUG_OBJECT(gvaclassify, "gst_gva_classify_init");
    GST_DEBUG_OBJECT(gvaclassify, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvaclassify)));

    if (gvaclassify == NULL)
        return;
    gst_gva_classify_cleanup(gvaclassify);

    gvaclassify->base_inference.is_full_frame = FALSE;
    gvaclassify->object_class = g_strdup(DEFAULT_OBJECT_CLASS);
    gvaclassify->reclassify_interval = DEFAULT_RECLASSIFY_INTERVAL;
    gvaclassify->classification_history = create_classification_history(gvaclassify);
    if (gvaclassify->classification_history == NULL)
        return;

    gvaclassify->base_inference.get_roi_pre_proc = INPUT_PRE_PROCESS;
    gvaclassify->base_inference.post_proc = EXTRACT_CLASSIFICATION_RESULTS;
    gvaclassify->base_inference.is_roi_classification_needed = IS_ROI_CLASSIFICATION_NEEDED;
}

void gst_gva_classify_cleanup(GstGvaClassify *gvaclassify) {
    if (gvaclassify == NULL)
        return;

    GST_DEBUG_OBJECT(gvaclassify, "gva_classify_cleanup");

    if (gvaclassify->classification_history) {
        release_classification_history(gvaclassify->classification_history);
        gvaclassify->classification_history = NULL;
    }

    g_free(gvaclassify->object_class);
    gvaclassify->object_class = NULL;
}

void gst_gva_classify_finalize(GObject *object) {
    GstGvaClassify *gvaclassify = GST_GVA_CLASSIFY(object);

    GST_DEBUG_OBJECT(gvaclassify, "finalize");

    gst_gva_classify_cleanup(gvaclassify);

    G_OBJECT_CLASS(gst_gva_classify_parent_class)->finalize(object);
}

GstPadProbeReturn FillROIParamsCallback(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    UNUSED(pad);
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer != NULL && user_data != NULL)
        fill_roi_params_from_history((struct ClassificationHistory *)user_data, buffer);

    return GST_PAD_PROBE_OK;
}
