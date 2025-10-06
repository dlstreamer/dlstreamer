/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "roi_split.h"

#include "dlstreamer/gst/frame.h"
#include "dlstreamer/image_metadata.h"
#include "gst/analytics/analytics.h"
#include "metadata/gva_tensor_meta.h"
#include "region_of_interest.h"

namespace {
GstFlowReturn send_gap_event(GstPad *pad, GstBuffer *buf) {
    auto gap_event = gst_event_new_gap(GST_BUFFER_PTS(buf), GST_BUFFER_DURATION(buf));
    return gst_pad_push_event(pad, gap_event) ? GST_BASE_TRANSFORM_FLOW_DROPPED : GST_FLOW_ERROR;
}
} // namespace

GST_DEBUG_CATEGORY_STATIC(roi_split_debug_category);
#define GST_CAT_DEFAULT roi_split_debug_category

G_DEFINE_TYPE_WITH_CODE(RoiSplit, roi_split, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(roi_split_debug_category, "roi_split", 0,
                                                "debug category for roi_split"));

enum { PROP_0, PROP_OBJECT_CLASS };

static void roi_split_init(RoiSplit *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
}

static void roi_split_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    RoiSplit *self = ROI_SPLIT(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_OBJECT_CLASS:
        self->object_classes = std::move(dlstreamer::split_string(g_value_get_string(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void roi_split_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    RoiSplit *self = ROI_SPLIT(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
    std::string str;
    switch (prop_id) {
    case PROP_OBJECT_CLASS:
        str = dlstreamer::join_strings(self->object_classes.cbegin(), self->object_classes.cend());
        g_value_set_string(value, str.c_str());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void roi_split_finalize(GObject *object) {
    RoiSplit *self = ROI_SPLIT(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(roi_split_parent_class)->finalize(object);
}

static gboolean roi_split_sink_event(GstBaseTransform *base, GstEvent *event) {
    RoiSplit *self = ROI_SPLIT(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return GST_BASE_TRANSFORM_CLASS(roi_split_parent_class)->sink_event(base, event);
}

static GstFlowReturn roi_split_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    RoiSplit *self = ROI_SPLIT(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Filter ROIs by object class
    GstAnalyticsODMtd od_mtd;
    GstVideoRegionOfInterestMeta *roi_meta;
    std::vector<GstAnalyticsODMtd> rois;
    gpointer state = nullptr;

    GstAnalyticsRelationMeta *relation_meta = gst_buffer_get_analytics_relation_meta(buf);
    if (!relation_meta) {
        GST_DEBUG_OBJECT(self, "No Relation meta. Push GAP event: ts=%" GST_TIME_FORMAT,
                         GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        return send_gap_event(base->srcpad, buf);
    }

    while (gst_analytics_relation_meta_iterate(relation_meta, &state, gst_analytics_od_mtd_get_mtd_type(), &od_mtd)) {
        GQuark label_quark = gst_analytics_od_mtd_get_obj_type(&od_mtd);
        if (label_quark) {
            auto &classes = self->object_classes;
            std::string name = g_quark_to_string(label_quark);
            if (std::find(classes.begin(), classes.end(), name) == classes.end())
                continue;
        }
        rois.push_back(od_mtd);
    }

    if (rois.empty()) {
        GST_DEBUG_OBJECT(self, "No ROI meta. Push GAP event: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        return send_gap_event(base->srcpad, buf);
    }

    // Create new buffers per ROI and push
    for (size_t i = 0; i < rois.size(); i++) {
        od_mtd = rois[i];

        // Create separate buffers from buf for each ROI meta
        GstBuffer *roi_buf = gst_buffer_new();

        // Copy everything
        if (!gst_buffer_copy_into(roi_buf, buf, GST_BUFFER_COPY_ALL, 0, static_cast<gsize>(-1))) {
            GST_ERROR_OBJECT(self, "Failed to copy data from input buffer into roi buffer");
            gst_buffer_unref(roi_buf);
            return GST_FLOW_ERROR;
        }

        gint x, y, w, h;
        gfloat r;

        if (!gst_analytics_od_mtd_get_oriented_location(&od_mtd, &x, &y, &w, &h, &r, nullptr)) {
            GST_ERROR_OBJECT(self, "Failed to get oriented location from od_mtd");
            gst_buffer_unref(roi_buf);
            return GST_FLOW_ERROR;
        }

        // attach VideoCropMeta
        auto crop_meta = gst_buffer_add_video_crop_meta(roi_buf);
        crop_meta->x = x;
        crop_meta->y = y;
        crop_meta->width = w;
        crop_meta->height = h;

        // attach SourceIdentifierMetadata
        auto meta = GST_GVA_TENSOR_META_ADD(roi_buf);
        gst_structure_set_name(meta->data, dlstreamer::SourceIdentifierMetadata::name);

        roi_meta = gst_buffer_get_video_region_of_interest_meta_id(buf, od_mtd.id);
        if (!roi_meta) {
            GST_ERROR_OBJECT(self, "Failed to get ROI meta by id %u", od_mtd.id);
            gst_buffer_unref(roi_buf);
            return GST_FLOW_ERROR;
        }

        GVA::RegionOfInterest gva_roi(od_mtd, roi_meta);
        gst_structure_set(meta->data, dlstreamer::SourceIdentifierMetadata::key::roi_id, G_TYPE_INT,
                          gva_roi.region_id(), dlstreamer::SourceIdentifierMetadata::key::object_id, G_TYPE_INT,
                          gva_roi.object_id(), dlstreamer::SourceIdentifierMetadata::key::pts, G_TYPE_POINTER,
                          static_cast<intptr_t>(GST_BUFFER_PTS(buf)), NULL);

        if (i == rois.size() - 1) { // set custom flag on last buffer
            gst_buffer_set_flags(roi_buf, static_cast<GstBufferFlags>(DLS_BUFFER_FLAG_LAST_ROI_ON_FRAME));
        }

        // push
        GST_DEBUG_OBJECT(self, "Push ROI buffer: id=%u ts=%" GST_TIME_FORMAT, od_mtd.id,
                         GST_TIME_ARGS(GST_BUFFER_PTS(roi_buf)));
        if (gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(base), roi_buf) != GST_FLOW_OK) {
            GST_ERROR_OBJECT(self, "Failed to push ROI buffer");
            return GST_FLOW_ERROR;
        }
    }

    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static void roi_split_class_init(RoiSplitClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = roi_split_set_property;
    gobject_class->get_property = roi_split_get_property;
    gobject_class->finalize = roi_split_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->sink_event = roi_split_sink_event;
    base_transform_class->transform_ip = roi_split_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, ROI_SPLIT_NAME, "application", ROI_SPLIT_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_CAPS_ANY));
    gst_element_class_add_pad_template(element_class,
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_CAPS_ANY));

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "object-class",
                            "Filter ROI list by object class(es) (comma separated list if multiple). Output only ROIs "
                            "with specified object class(es)",
                            "", static_cast<GParamFlags>(G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)));
}
