/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvawatermark.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "fps_meter.h"
#include "gva_roi_meta.h"
#include "watermark.h"

#include "config.h"
#include <stdio.h>

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME "Draw detection/classification/recognition results on top of video data"
#define ELEMENT_DESCRIPTION "Draw detection/classification/recognition results on top of video data"

GST_DEBUG_CATEGORY_STATIC(gst_gva_watermark_debug_category);
#define GST_CAT_DEFAULT gst_gva_watermark_debug_category

/* prototypes */
static void gst_gva_watermark_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_watermark_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_watermark_dispose(GObject *object);
static void gst_gva_watermark_finalize(GObject *object);

static gboolean gst_gva_watermark_start(GstBaseTransform *trans);
static gboolean gst_gva_watermark_stop(GstBaseTransform *trans);
static gboolean gst_gva_watermark_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static GstFlowReturn gst_gva_watermark_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

enum { PROP_0 };

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define VA_SURFACE_CAPS

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")

#define WATERMARK_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS WATERMARK_CAPS
#define VIDEO_SRC_CAPS WATERMARK_CAPS

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaWatermark, gst_gva_watermark, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_watermark_debug_category, "gvawatermark", 0,
                                                "debug category for gvawatermark element"));

static void gst_gva_watermark_class_init(GstGvaWatermarkClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_watermark_set_property;
    gobject_class->get_property = gst_gva_watermark_get_property;
    gobject_class->dispose = gst_gva_watermark_dispose;
    gobject_class->finalize = gst_gva_watermark_finalize;
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_watermark_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_watermark_stop);
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_watermark_set_caps);
    base_transform_class->transform = NULL;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_watermark_transform_ip);
}

static void gst_gva_watermark_init(GstGvaWatermark *gvawatermark) {
    UNUSED(gvawatermark);
}

void gst_gva_watermark_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    UNUSED(value);

    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "set_property");

    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_watermark_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    UNUSED(value);

    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "get_property");

    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_watermark_dispose(GObject *object) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_watermark_parent_class)->dispose(object);
}

void gst_gva_watermark_finalize(GObject *object) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "finalize");
    /* clean up object here */

    G_OBJECT_CLASS(gst_gva_watermark_parent_class)->finalize(object);
}

static gboolean gst_gva_watermark_start(GstBaseTransform *trans) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(trans);

    GST_DEBUG_OBJECT(gvawatermark, "start");
    fps_meter_init(&gvawatermark->fps_meter);
    return TRUE;
}

static gboolean gst_gva_watermark_stop(GstBaseTransform *trans) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(trans);

    GST_DEBUG_OBJECT(gvawatermark, "stop");

    return TRUE;
}

static gboolean gst_gva_watermark_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(trans);

    GST_DEBUG_OBJECT(gvawatermark, "set_caps");

    gst_video_info_from_caps(&gvawatermark->info, incaps);

    return TRUE;
}

static GstFlowReturn gst_gva_watermark_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(trans);

    GST_DEBUG_OBJECT(gvawatermark, "transform_ip");

    if (fps_meter_new_frame(&gvawatermark->fps_meter, 1000)) {
        GST_INFO_OBJECT(gvawatermark, "fps %.2f", gvawatermark->fps_meter.fps);
    }
    if (!gst_pad_is_linked(GST_BASE_TRANSFORM_SRC_PAD(trans))) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    draw_label(buf, &gvawatermark->info);

    return GST_FLOW_OK;
}
