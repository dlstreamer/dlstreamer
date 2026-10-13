/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaroiscale.h"

#include <cstring>

#include <gst/video/video.h>

#include "roi_scale.hpp"

constexpr guint DEFAULT_OUT_WIDTH = 448;
constexpr guint DEFAULT_OUT_HEIGHT = 448;
constexpr guint MAX_OUT_DIMENSION = 8192;

enum { PROP_0, PROP_OUT_WIDTH, PROP_OUT_HEIGHT };

struct GstGvaRoiScale {
    GstBaseTransform parent;
    guint out_width;
    guint out_height;
    GstVideoInfo input_info;
    GstVideoInfo output_info;
};

struct GstGvaRoiScaleClass {
    GstBaseTransformClass parent_class;
};

G_DEFINE_TYPE(GstGvaRoiScale, gst_gva_roi_scale, GST_TYPE_BASE_TRANSFORM)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, format=(string)RGB, width=(int)[1,8192], height=(int)[1,8192]"));
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, format=(string)RGB, width=(int)[1,8192], height=(int)[1,8192]"));

static GstCaps *gst_gva_roi_scale_transform_caps(GstBaseTransform *transform, GstPadDirection direction,
                                                 GstCaps *caps, GstCaps *filter) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    GstCaps *result = gst_caps_copy(caps);
    for (guint i = 0; i < gst_caps_get_size(result); ++i) {
        GstStructure *structure = gst_caps_get_structure(result, i);
        if (direction == GST_PAD_SINK) {
            gst_structure_set(structure, "width", G_TYPE_INT, static_cast<gint>(self->out_width), "height", G_TYPE_INT,
                              static_cast<gint>(self->out_height), nullptr);
        } else {
            gst_structure_remove_field(structure, "width");
            gst_structure_remove_field(structure, "height");
        }
    }
    if (filter) {
        GstCaps *intersection = gst_caps_intersect_full(filter, result, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(result);
        result = intersection;
    }
    return result;
}

static gboolean gst_gva_roi_scale_set_caps(GstBaseTransform *transform, GstCaps *input_caps, GstCaps *output_caps) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    if (!gst_video_info_from_caps(&self->input_info, input_caps) ||
        !gst_video_info_from_caps(&self->output_info, output_caps) ||
        GST_VIDEO_INFO_FORMAT(&self->input_info) != GST_VIDEO_FORMAT_RGB ||
        GST_VIDEO_INFO_FORMAT(&self->output_info) != GST_VIDEO_FORMAT_RGB ||
        GST_VIDEO_INFO_WIDTH(&self->output_info) != self->out_width ||
        GST_VIDEO_INFO_HEIGHT(&self->output_info) != self->out_height)
        return FALSE;
    return TRUE;
}

static GstFlowReturn gst_gva_roi_scale_transform(GstBaseTransform *transform, GstBuffer *input, GstBuffer *output) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    GstBuffer *scaled = nullptr;
    GstVideoInfo scaled_info;
    const auto result = gvaroiscale::roi_scale_buffer(input, self->input_info, self->out_width, self->out_height,
                                                       &scaled, &scaled_info);
    if (result == gvaroiscale::RoiScaleResult::NoRoi)
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    if (result != gvaroiscale::RoiScaleResult::Ok)
        return GST_FLOW_ERROR;

    GstMapInfo source_map = GST_MAP_INFO_INIT;
    GstMapInfo output_map = GST_MAP_INFO_INIT;
    if (!gst_buffer_map(scaled, &source_map, GST_MAP_READ) || !gst_buffer_map(output, &output_map, GST_MAP_WRITE)) {
        if (source_map.data)
            gst_buffer_unmap(scaled, &source_map);
        gst_buffer_unref(scaled);
        return GST_FLOW_ERROR;
    }
    if (source_map.size != output_map.size) {
        gst_buffer_unmap(output, &output_map);
        gst_buffer_unmap(scaled, &source_map);
        gst_buffer_unref(scaled);
        return GST_FLOW_ERROR;
    }

    std::memcpy(output_map.data, source_map.data, source_map.size);
    gst_buffer_unmap(output, &output_map);
    gst_buffer_unmap(scaled, &source_map);
    gst_buffer_unref(scaled);
    return GST_FLOW_OK;
}

static void gst_gva_roi_scale_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(object);
    switch (property_id) {
    case PROP_OUT_WIDTH:
        self->out_width = g_value_get_uint(value);
        break;
    case PROP_OUT_HEIGHT:
        self->out_height = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        return;
    }
    gst_base_transform_reconfigure_src(GST_BASE_TRANSFORM(self));
}

static void gst_gva_roi_scale_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(object);
    switch (property_id) {
    case PROP_OUT_WIDTH:
        g_value_set_uint(value, self->out_width);
        break;
    case PROP_OUT_HEIGHT:
        g_value_set_uint(value, self->out_height);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void gst_gva_roi_scale_class_init(GstGvaRoiScaleClass *klass) {
    auto *object_class = G_OBJECT_CLASS(klass);
    auto *transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    auto *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_static_pad_template(element_class, &sink_template);
    gst_element_class_add_static_pad_template(element_class, &src_template);
    gst_element_class_set_static_metadata(element_class, "ROI Scale", "Video/Filter",
                                          "Crops the detected ROI and scales it to the configured VLM input size",
                                          "Intel Corporation");
    object_class->set_property = gst_gva_roi_scale_set_property;
    object_class->get_property = gst_gva_roi_scale_get_property;
    transform_class->transform_caps = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_transform_caps);
    transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_set_caps);
    transform_class->transform = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_transform);

    const auto property_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_OUT_WIDTH,
                                    g_param_spec_uint("out-width", "Output Width", "Scaled ROI output width", 1,
                                                      MAX_OUT_DIMENSION, DEFAULT_OUT_WIDTH, property_flags));
    g_object_class_install_property(object_class, PROP_OUT_HEIGHT,
                                    g_param_spec_uint("out-height", "Output Height", "Scaled ROI output height", 1,
                                                      MAX_OUT_DIMENSION, DEFAULT_OUT_HEIGHT, property_flags));
}

static void gst_gva_roi_scale_init(GstGvaRoiScale *self) {
    self->out_width = DEFAULT_OUT_WIDTH;
    self->out_height = DEFAULT_OUT_HEIGHT;
    gst_video_info_init(&self->input_info);
    gst_video_info_init(&self->output_info);
    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), FALSE);
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
}
