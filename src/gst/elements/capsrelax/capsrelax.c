/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "capsrelax.h"

#define UNUSED_PARAMETER(param) (void)(param)

#define CAPS_WIDTH_FIELD "width"
#define CAPS_HEIGHT_FIELD "height"

GST_DEBUG_CATEGORY_STATIC(gst_capsrelax_debug);
#define GST_CAT_DEFAULT gst_capsrelax_debug

#define _do_init GST_DEBUG_CATEGORY_INIT(gst_capsrelax_debug, "capsrelax", 0, "capsrelax element");
#define gst_capsrelax_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstCapsRelax, gst_capsrelax, GST_TYPE_BASE_TRANSFORM, _do_init);

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

static gboolean caps_is_resolution_fixed(GstCapsFeatures *features, GstStructure *structure, gpointer userdata) {
    UNUSED_PARAMETER(features);
    UNUSED_PARAMETER(userdata);

    const GValue *value = gst_structure_get_value(structure, CAPS_WIDTH_FIELD);
    if (value && gst_value_is_fixed(value))
        return TRUE;
    value = gst_structure_get_value(structure, CAPS_HEIGHT_FIELD);
    if (value && gst_value_is_fixed(value))
        return TRUE;
    return FALSE;
}

static gboolean caps_filter_fixed_resolution(GstCapsFeatures *features, GstStructure *structure, gpointer userdata) {
    UNUSED_PARAMETER(features);
    UNUSED_PARAMETER(userdata);

    const GValue *value = gst_structure_get_value(structure, CAPS_WIDTH_FIELD);
    if (value && gst_value_is_fixed(value))
        return FALSE;
    value = gst_structure_get_value(structure, CAPS_HEIGHT_FIELD);
    if (value && gst_value_is_fixed(value))
        return FALSE;
    return TRUE;
}

static void gst_capsrelax_init(GstCapsRelax *self) {
    gst_base_transform_set_prefer_passthrough(GST_BASE_TRANSFORM(self), FALSE);
}

static GstCaps *gst_capsrelax_transform_caps(GstBaseTransform *base, GstPadDirection direction, GstCaps *caps,
                                             GstCaps *filter) {
    GstCaps *caps_result = GST_BASE_TRANSFORM_CLASS(parent_class)->transform_caps(base, direction, caps, filter);

    GST_DEBUG_OBJECT(base, "input: %" GST_PTR_FORMAT, caps);
    GST_DEBUG_OBJECT(base, "filter: %" GST_PTR_FORMAT, filter);
    GST_DEBUG_OBJECT(base, "result: %" GST_PTR_FORMAT, caps_result);
    GST_DEBUG_OBJECT(base, "direction: %d", direction);

    // Do not pass fixed resolution to upstream (width/height).
    // So if any scaling is needed is should be performed by downstream
    // However, if only fixed caps are present, they will be passed without modification
    if (direction == GST_PAD_SRC) {
        gboolean has_range = !gst_caps_foreach(caps_result, caps_is_resolution_fixed, NULL);

        if (has_range) {
            caps_result = gst_caps_make_writable(caps_result);
            gst_caps_filter_and_map_in_place(caps_result, caps_filter_fixed_resolution, NULL);
            GST_DEBUG_OBJECT(base, "relaxed caps: %" GST_PTR_FORMAT, caps_result);
        }
    }

    return caps_result;
}

static GstFlowReturn gst_capsrelax_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    UNUSED_PARAMETER(base);
    UNUSED_PARAMETER(buf);
    return GST_FLOW_OK;
}

static void gst_capsrelax_class_init(GstCapsRelaxClass *klass) {
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);
    GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS(klass);

    gst_element_class_set_static_metadata(gstelement_class, "CapsRelax", "Generic",
                                          "Pass data without modification, relaxes formats", "Intel Corporation");

    gst_element_class_add_static_pad_template(gstelement_class, &sink_template);
    gst_element_class_add_static_pad_template(gstelement_class, &src_template);

    trans_class->transform_caps = gst_capsrelax_transform_caps;
    trans_class->transform_ip = gst_capsrelax_transform_ip;
}
