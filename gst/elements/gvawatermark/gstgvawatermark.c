/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvawatermark.h"

#include <gst/video/video.h>

#include "gva_caps.h"

#include <stdio.h>

#define ELEMENT_LONG_NAME "Bin element for detection/classification/recognition results labeling"
#define ELEMENT_DESCRIPTION "Overlays the metadata on the video frame to visualize the inference results."

GST_DEBUG_CATEGORY_STATIC(gst_gva_watermark_debug_category);
#define GST_CAT_DEFAULT gst_gva_watermark_debug_category

#define DEFAULT_DEVICE NULL

enum { PROP_0, PROP_DEVICE };

G_DEFINE_TYPE_WITH_CODE(GstGvaWatermark, gst_gva_watermark, GST_TYPE_BIN,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_watermark_debug_category, "gvawatermark", 0,
                                                "debug category for gvawatermark element"));

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(GVA_CAPS));

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(GVA_CAPS));

static void gst_gva_watermark_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_watermark_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_watermark_finalize(GObject *object);
static gboolean gst_gva_watermark_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);
static gboolean gva_watermark_link_elements(GstGvaWatermark *self, GstCaps *caps);
static gboolean gva_watermark_create_vaapipostproc(GstGvaWatermark *self);

static void gst_gva_watermark_class_init(GstGvaWatermarkClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_static_pad_template(element_class, &sinktemplate);
    gst_element_class_add_static_pad_template(element_class, &srctemplate);

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_watermark_set_property;
    gobject_class->get_property = gst_gva_watermark_get_property;
    gobject_class->finalize = gst_gva_watermark_finalize;

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Target device",
            "Supported devices are CPU and GPU. Default is CPU on system memory and GPU on video memory",
            DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_watermark_init(GstGvaWatermark *gvawatermark) {
    GstPadTemplate *pad_tmpl;

    pad_tmpl = gst_static_pad_template_get(&sinktemplate);
    gvawatermark->sinkpad = gst_ghost_pad_new_no_target_from_template("sink", pad_tmpl);
    gst_object_unref(pad_tmpl);

    pad_tmpl = gst_static_pad_template_get(&srctemplate);
    gvawatermark->srcpad = gst_ghost_pad_new_no_target_from_template("src", pad_tmpl);
    gst_object_unref(pad_tmpl);

    gst_pad_set_event_function(gvawatermark->sinkpad, GST_DEBUG_FUNCPTR(gst_gva_watermark_sink_event));

    gst_element_add_pad(GST_ELEMENT(gvawatermark), gvawatermark->sinkpad);
    gst_element_add_pad(GST_ELEMENT(gvawatermark), gvawatermark->srcpad);

    gvawatermark->watermarkimpl = gst_element_factory_make("gvawatermarkimpl", "gvawatermarkimpl");
    if (!gvawatermark->watermarkimpl)
        GST_ERROR_OBJECT(gvawatermark, "Could not create gvawatermark instance");

    gst_bin_add(GST_BIN(gvawatermark), gst_object_ref(gvawatermark->watermarkimpl));

    GstPad *pad = gst_element_get_static_pad(gvawatermark->watermarkimpl, "src");
    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(gvawatermark->srcpad), pad);
    gst_object_unref(pad);

    gvawatermark->vaapipostproc = NULL;
    gvawatermark->device = DEFAULT_DEVICE;
    gvawatermark->current_sink = NULL;
}

void gst_gva_watermark_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "set_property");

    switch (prop_id) {
    case PROP_DEVICE:
        g_free(gvawatermark->device);
        gvawatermark->device = g_value_dup_string(value);
        g_object_set(gvawatermark->watermarkimpl, "device", gvawatermark->device, NULL);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_watermark_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "get_property");

    switch (prop_id) {
    case PROP_DEVICE:
        g_value_set_string(value, gvawatermark->device);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_watermark_finalize(GObject *object) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "finalize");

    if (gvawatermark->watermarkimpl) {
        gst_object_unref(gvawatermark->watermarkimpl);
        gvawatermark->watermarkimpl = NULL;
    }

    if (gvawatermark->vaapipostproc) {
        gst_object_unref(gvawatermark->vaapipostproc);
        gvawatermark->vaapipostproc = NULL;
    }

    g_free(gvawatermark->device);
    gvawatermark->device = NULL;

    G_OBJECT_CLASS(gst_gva_watermark_parent_class)->finalize(object);
}

static gboolean gst_gva_watermark_sink_event(GstPad *pad, GstObject *parent, GstEvent *event) {
    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);

        GstGvaWatermark *self = GST_GVA_WATERMARK(parent);
        if (!gva_watermark_link_elements(self, caps)) {
            gst_event_unref(event);
            return FALSE;
        }
    }

    return gst_pad_event_default(pad, parent, event);
}

static gboolean gva_watermark_link_elements(GstGvaWatermark *self, GstCaps *caps) {
    GST_DEBUG_OBJECT(self, "Link elements, sink caps: %" GST_PTR_FORMAT, caps);
    if (get_caps_feature(caps) == VA_SURFACE_CAPS_FEATURE) {
        if (!gva_watermark_create_vaapipostproc(self)) {
            GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't create vaapipostproc element."), (NULL));
            return FALSE;
        }

        // Check if we already have right sink element
        if (self->current_sink == self->vaapipostproc) {
            GST_DEBUG_OBJECT(self, "Link elements, skip -> already linked");
            return TRUE;
        }

        gst_element_set_locked_state(self->vaapipostproc, FALSE);
        if (!gst_element_sync_state_with_parent(self->vaapipostproc)) {
            GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't sync vaapipostproc element state with parent."),
                              (NULL));
            return FALSE;
        }

        // Unlink proxy pad
        gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->sinkpad), NULL);

        // Link watermark to vaapipostproc.
        if (!gst_element_link_pads(self->vaapipostproc, "src", self->watermarkimpl, "sink")) {
            GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't link elements."), (NULL));
            return FALSE;
        }

        self->current_sink = self->vaapipostproc;
    } else {
        if (self->current_sink == self->watermarkimpl) {
            GST_DEBUG_OBJECT(self, "Link elements, skip -> already linked");
            return TRUE;
        }

        // Unlink proxy pad
        gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->sinkpad), NULL);

        if (self->vaapipostproc) {
            gst_element_set_state(self->vaapipostproc, GST_STATE_NULL);
            gst_element_set_locked_state(self->vaapipostproc, TRUE);
            gst_element_unlink(self->vaapipostproc, self->watermarkimpl);
        }

        self->current_sink = self->watermarkimpl;
    }

    // Set sink proxy pad
    g_assert(self->current_sink);
    GstPad *sinkpad = gst_element_get_static_pad(self->current_sink, "sink");

    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->sinkpad), sinkpad);
    GST_DEBUG_OBJECT(self, "Link elements, proxy pad set %" GST_PTR_FORMAT, sinkpad);
    gst_object_unref(sinkpad);
    return TRUE;
}

gboolean gva_watermark_create_vaapipostproc(GstGvaWatermark *self) {
    if (self->vaapipostproc)
        return TRUE;

    self->vaapipostproc = gst_element_factory_make("vaapipostproc", "vaapipostproc");
    if (!self->vaapipostproc) {
        GST_ERROR_OBJECT(self, "Couldn't create vaapipostproc instance");
        return FALSE;
    }

    g_object_set(self->vaapipostproc, "brightness", 0.00001f, NULL);

    gboolean ret = gst_bin_add(GST_BIN(self), gst_object_ref(self->vaapipostproc));
    return ret;
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvawatermark, "DL Streamer gvawatermark element", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
