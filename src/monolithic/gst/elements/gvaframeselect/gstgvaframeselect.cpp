/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaframeselect.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "frame_selector.hpp"
#include "gva_caps.h"

constexpr guint DEFAULT_SUB_STREAM_FRAMES = 900;
constexpr guint DEFAULT_FRAME_BUDGET = 8;

enum { PROP_0, PROP_SUB_STREAM_FRAMES, PROP_FRAME_BUDGET };

struct GstGvaFrameSelect {
    GstBaseTransform parent;
    guint sub_stream_frames;
    guint frame_budget;
    guint frame_index;
};

struct GstGvaFrameSelectClass {
    GstBaseTransformClass parent_class;
};

G_DEFINE_TYPE(GstGvaFrameSelect, gst_gva_frame_select, GST_TYPE_BASE_TRANSFORM)

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(GVA_CAPS));
static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(GVA_CAPS));

static void gst_gva_frame_select_set_property(GObject *object, guint property_id, const GValue *value,
                                              GParamSpec *pspec) {
    auto *self = reinterpret_cast<GstGvaFrameSelect *>(object);
    switch (property_id) {
    case PROP_SUB_STREAM_FRAMES:
        self->sub_stream_frames = g_value_get_uint(value);
        self->frame_index = 0;
        break;
    case PROP_FRAME_BUDGET:
        self->frame_budget = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void gst_gva_frame_select_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    auto *self = reinterpret_cast<GstGvaFrameSelect *>(object);
    switch (property_id) {
    case PROP_SUB_STREAM_FRAMES:
        g_value_set_uint(value, self->sub_stream_frames);
        break;
    case PROP_FRAME_BUDGET:
        g_value_set_uint(value, self->frame_budget);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static GstFlowReturn gst_gva_frame_select_transform_ip(GstBaseTransform *transform, GstBuffer *buffer) {
    auto *self = reinterpret_cast<GstGvaFrameSelect *>(transform);
    if (GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT))
        self->frame_index = 0;

    const guint index = self->frame_index++ % self->sub_stream_frames;
    const auto selected = gvaframeselect::select_indices(self->sub_stream_frames, self->frame_budget);
    return std::binary_search(selected.begin(), selected.end(), static_cast<std::size_t>(index))
               ? GST_FLOW_OK
               : GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static void gst_gva_frame_select_class_init(GstGvaFrameSelectClass *klass) {
    auto *object_class = G_OBJECT_CLASS(klass);
    auto *transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    auto *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_static_pad_template(element_class, &sink_template);
    gst_element_class_add_static_pad_template(element_class, &src_template);
    gst_element_class_set_static_metadata(element_class, "Frame Select", "Video/Filter",
                                          "Selects a fixed frame budget from each event sub-stream", "Intel Corporation");

    object_class->set_property = gst_gva_frame_select_set_property;
    object_class->get_property = gst_gva_frame_select_get_property;
    transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_frame_select_transform_ip);

    g_object_class_install_property(
        object_class, PROP_SUB_STREAM_FRAMES,
        g_param_spec_uint("sub-stream-frames", "Sub-stream Frames",
                          "Length N, in input frames, of each selection block; the pattern restarts every N frames "
                          "and on DISCONT",
                          1, G_MAXUINT, DEFAULT_SUB_STREAM_FRAMES,
                          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(
        object_class, PROP_FRAME_BUDGET,
        g_param_spec_uint("frame-budget", "Frame Budget",
                          "Maximum k frames forwarded per full block; selected positions are evenly spaced, with the "
                          "first frame used when k is 1",
                          1, G_MAXUINT, DEFAULT_FRAME_BUDGET,
                          static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_gva_frame_select_init(GstGvaFrameSelect *self) {
    self->sub_stream_frames = DEFAULT_SUB_STREAM_FRAMES;
    self->frame_budget = DEFAULT_FRAME_BUDGET;
    self->frame_index = 0;
    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
}
