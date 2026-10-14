/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaroiscale.h"

#include <cstring>

#include <glib/gstdio.h>
#include <gst/video/video.h>

#include "roi_scale.hpp"

constexpr guint DEFAULT_OUT_WIDTH = 448;
constexpr guint DEFAULT_OUT_HEIGHT = 448;
constexpr guint MAX_OUT_DIMENSION = 8192;
constexpr GstClockTime JPEG_ENCODE_TIMEOUT = 5 * GST_SECOND;

enum { PROP_0, PROP_OUT_WIDTH, PROP_OUT_HEIGHT, PROP_OUTPUT_DIR, PROP_TARGET_CLASSES };

struct GstGvaRoiScale {
    GstBaseTransform parent;
    guint out_width;
    guint out_height;
    guint64 output_frame_number;
    gchar *output_dir;
    gchar *target_classes_csv;
    GArray *target_classes;
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

static gboolean gst_gva_roi_scale_get_unit_size(GstBaseTransform *, GstCaps *caps, gsize *size) {
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, caps))
        return FALSE;
    *size = info.size;
    return TRUE;
}

static GArray *parse_target_classes(const gchar *csv) {
    GArray *classes = g_array_new(FALSE, FALSE, sizeof(GQuark));
    if (!csv || !*csv)
        return classes;

    gchar **labels = g_strsplit(csv, ",", -1);
    for (gchar **label = labels; *label; ++label) {
        gchar *trimmed = g_strstrip(*label);
        if (!*trimmed)
            continue;
        const GQuark quark = g_quark_from_string(trimmed);
        g_array_append_val(classes, quark);
    }
    g_strfreev(labels);
    return classes;
}

static void save_jpeg(GstGvaRoiScale *self, GstBuffer *buffer, const gchar *output_dir, guint64 frame_number) {
    if (g_mkdir_with_parents(output_dir, 0755) != 0) {
        GST_ELEMENT_WARNING(self, RESOURCE, OPEN_WRITE, ("Could not create output directory '%s'", output_dir),
                            (NULL));
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SRC_PAD(self));
    if (!caps) {
        GST_ELEMENT_WARNING(self, STREAM, ENCODE, ("Could not get output caps for frame %" G_GUINT64_FORMAT,
                                                  frame_number),
                            (NULL));
        return;
    }
    GstSample *sample = gst_sample_new(buffer, caps, nullptr, nullptr);
    gst_caps_unref(caps);
    GstCaps *jpeg_caps = gst_caps_new_empty_simple("image/jpeg");
    GError *error = nullptr;
    GstSample *jpeg = gst_video_convert_sample(sample, jpeg_caps, JPEG_ENCODE_TIMEOUT, &error);
    gst_caps_unref(jpeg_caps);
    gst_sample_unref(sample);
    if (!jpeg) {
        GST_ELEMENT_WARNING(self, STREAM, ENCODE, ("Could not encode frame %" G_GUINT64_FORMAT " to JPEG", frame_number),
                            ("%s", error ? error->message : "unknown error"));
        g_clear_error(&error);
        return;
    }

    GstBuffer *jpeg_buffer = gst_sample_get_buffer(jpeg);
    GstMapInfo map = GST_MAP_INFO_INIT;
    if (jpeg_buffer && gst_buffer_map(jpeg_buffer, &map, GST_MAP_READ)) {
        gchar *file_name = g_strdup_printf("frame_%08" G_GUINT64_FORMAT ".jpg", frame_number);
        gchar *path = g_build_filename(output_dir, file_name, nullptr);
        if (!g_file_set_contents(path, reinterpret_cast<const gchar *>(map.data), static_cast<gssize>(map.size),
                                 &error)) {
            GST_ELEMENT_WARNING(self, RESOURCE, WRITE, ("Could not write '%s'", path),
                                ("%s", error ? error->message : "unknown error"));
            g_clear_error(&error);
        }
        g_free(path);
        g_free(file_name);
        gst_buffer_unmap(jpeg_buffer, &map);
    }
    gst_sample_unref(jpeg);
}

static GstFlowReturn gst_gva_roi_scale_transform(GstBaseTransform *transform, GstBuffer *input, GstBuffer *output) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    GstBuffer *scaled = nullptr;
    GstVideoInfo scaled_info;
    GST_OBJECT_LOCK(self);
    GArray *target_classes = g_array_ref(self->target_classes);
    GST_OBJECT_UNLOCK(self);
    const auto result = gvaroiscale::roi_scale_buffer(input, self->input_info, self->out_width, self->out_height,
                                                       target_classes, &scaled, &scaled_info);
    g_array_unref(target_classes);
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

    const guint64 frame_number = self->output_frame_number++;
    GST_OBJECT_LOCK(self);
    gchar *output_dir = g_strdup(self->output_dir);
    GST_OBJECT_UNLOCK(self);
    if (output_dir && *output_dir)
        save_jpeg(self, output, output_dir, frame_number);
    g_free(output_dir);

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
    case PROP_OUTPUT_DIR:
        GST_OBJECT_LOCK(self);
        g_free(self->output_dir);
        self->output_dir = g_value_dup_string(value);
        GST_OBJECT_UNLOCK(self);
        break;
    case PROP_TARGET_CLASSES: {
        gchar *target_classes_csv = g_value_dup_string(value);
        GArray *target_classes = parse_target_classes(target_classes_csv);
        GST_OBJECT_LOCK(self);
        g_free(self->target_classes_csv);
        g_array_unref(self->target_classes);
        self->target_classes_csv = target_classes_csv;
        self->target_classes = target_classes;
        GST_OBJECT_UNLOCK(self);
        break;
    }
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
    case PROP_OUTPUT_DIR:
        GST_OBJECT_LOCK(self);
        g_value_set_string(value, self->output_dir);
        GST_OBJECT_UNLOCK(self);
        break;
    case PROP_TARGET_CLASSES:
        GST_OBJECT_LOCK(self);
        g_value_set_string(value, self->target_classes_csv);
        GST_OBJECT_UNLOCK(self);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void gst_gva_roi_scale_finalize(GObject *object) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(object);
    g_free(self->output_dir);
    g_free(self->target_classes_csv);
    g_array_unref(self->target_classes);
    G_OBJECT_CLASS(gst_gva_roi_scale_parent_class)->finalize(object);
}

static gboolean gst_gva_roi_scale_start(GstBaseTransform *transform) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    self->output_frame_number = 0;
    return TRUE;
}

static gboolean gst_gva_roi_scale_sink_event(GstBaseTransform *transform, GstEvent *event) {
    auto *self = reinterpret_cast<GstGvaRoiScale *>(transform);
    if (GST_EVENT_TYPE(event) == GST_EVENT_STREAM_START)
        self->output_frame_number = 0;
    return GST_BASE_TRANSFORM_CLASS(gst_gva_roi_scale_parent_class)->sink_event(transform, event);
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
    object_class->finalize = gst_gva_roi_scale_finalize;
    transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_start);
    transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_sink_event);
    transform_class->transform_caps = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_transform_caps);
    transform_class->get_unit_size = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_get_unit_size);
    transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_set_caps);
    transform_class->transform = GST_DEBUG_FUNCPTR(gst_gva_roi_scale_transform);

    const auto property_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(object_class, PROP_OUT_WIDTH,
                                    g_param_spec_uint("out-width", "Output Width", "Scaled ROI output width", 1,
                                                      MAX_OUT_DIMENSION, DEFAULT_OUT_WIDTH, property_flags));
    g_object_class_install_property(object_class, PROP_OUT_HEIGHT,
                                    g_param_spec_uint("out-height", "Output Height", "Scaled ROI output height", 1,
                                                      MAX_OUT_DIMENSION, DEFAULT_OUT_HEIGHT, property_flags));
    g_object_class_install_property(
        object_class, PROP_OUTPUT_DIR,
        g_param_spec_string("output-dir", "Output Directory",
                            "If set, each scaled ROI frame is also saved as a JPEG in this directory", nullptr,
                            property_flags));
    g_object_class_install_property(
        object_class, PROP_TARGET_CLASSES,
        g_param_spec_string("target-classes", "Target Classes",
                            "Comma-separated detection labels to consider; empty means all classes", "",
                            property_flags));
}

static void gst_gva_roi_scale_init(GstGvaRoiScale *self) {
    self->out_width = DEFAULT_OUT_WIDTH;
    self->out_height = DEFAULT_OUT_HEIGHT;
    self->output_frame_number = 0;
    self->output_dir = nullptr;
    self->target_classes_csv = g_strdup("");
    self->target_classes = g_array_new(FALSE, FALSE, sizeof(GQuark));
    gst_video_info_init(&self->input_info);
    gst_video_info_init(&self->output_info);
    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), FALSE);
    gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), FALSE);
}
