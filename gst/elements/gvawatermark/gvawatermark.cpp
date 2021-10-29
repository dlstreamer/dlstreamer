/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvawatermark.h"
#include "gvawatermarkcaps.h"

#include <gst/video/video.h>

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
static GstStateChangeReturn gva_watermark_change_state(GstElement *element, GstStateChange transition);

static void gst_gva_watermark_class_init(GstGvaWatermarkClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_static_pad_template(element_class, &sinktemplate);
    gst_element_class_add_static_pad_template(element_class, &srctemplate);

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    element_class->change_state = gva_watermark_change_state;

    gobject_class->set_property = gst_gva_watermark_set_property;
    gobject_class->get_property = gst_gva_watermark_get_property;
    gobject_class->finalize = gst_gva_watermark_finalize;

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Target device",
            "Supported devices are CPU and GPU. Default is CPU on system memory and GPU on video memory",
            DEFAULT_DEVICE, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_gva_watermark_init(GstGvaWatermark *self) {
    GstPadTemplate *pad_tmpl = gst_static_pad_template_get(&sinktemplate);
    self->sinkpad = gst_ghost_pad_new_no_target_from_template("sink", pad_tmpl);
    gst_object_unref(pad_tmpl);

    pad_tmpl = gst_static_pad_template_get(&srctemplate);
    self->srcpad = gst_ghost_pad_new_no_target_from_template("src", pad_tmpl);
    gst_object_unref(pad_tmpl);

    gst_pad_set_event_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_gva_watermark_sink_event));

    gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);
    gst_element_add_pad(GST_ELEMENT(self), self->srcpad);

    self->identity = gst_element_factory_make("identity", NULL);
    if (!self->identity)
        GST_ERROR_OBJECT(self, "Could not create identity instance");

    self->watermarkimpl = gst_element_factory_make("gvawatermarkimpl", NULL);
    if (!self->watermarkimpl)
        GST_ERROR_OBJECT(self, "Could not create gvawatermark instance");

    gst_bin_add_many(GST_BIN(self), GST_ELEMENT(gst_object_ref(self->watermarkimpl)),
                     GST_ELEMENT(gst_object_ref(self->identity)), NULL);

    self->device = DEFAULT_DEVICE;
    self->active_path = WatermarkPathNone;
    self->block_probe_id = 0;
    memset(self->vaapi_path_elems, 0, sizeof(self->vaapi_path_elems));
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

    g_free(gvawatermark->device);
    gvawatermark->device = NULL;

    G_OBJECT_CLASS(gst_gva_watermark_parent_class)->finalize(object);
}

static GstPadProbeReturn gva_watermark_sink_block_pad_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    (void)pad;
    (void)user_data;

    if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
        GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS || GST_EVENT_TYPE(event) == GST_EVENT_STREAM_START)
            return GST_PAD_PROBE_PASS;
    }
    return GST_PAD_PROBE_OK;
}

static void gva_watermark_block_sink(GstGvaWatermark *self, gboolean enable_block) {
    if (enable_block && self->block_probe_id != 0)
        return;
    if (!enable_block && self->block_probe_id == 0)
        return;

    GstPad *pad = gst_element_get_static_pad(self->identity, "sink");

    if (enable_block) {
        self->block_probe_id =
            gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, gva_watermark_sink_block_pad_probe, NULL, NULL);
    } else {
        gst_pad_remove_probe(pad, self->block_probe_id);
        self->block_probe_id = 0;
    }

    gst_object_unref(pad);
    GST_DEBUG_OBJECT(self, "Sink block set to: %d", enable_block);
}

static gboolean gva_watermark_set_src_pad(GstGvaWatermark *self, GstElement *src) {
    // Link
    GstPad *pad = gst_element_get_static_pad(src, "src");
    gboolean ret = gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->srcpad), pad);
    gst_object_unref(pad);
    return ret;
}

static gboolean gva_watermark_link_elements(GstGvaWatermark *self, GstElement *src, GstElement *dest) {
    if (!gst_element_link(src, dest)) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND,
                          ("Couldn't link element %s to %s", GST_ELEMENT_NAME(src), GST_ELEMENT_NAME(dest)), (NULL));
        return FALSE;
    }
    return TRUE;
}

static const gchar *get_caps_str_with_feature(CapsFeature mem_type) {
    if (mem_type == VA_SURFACE_CAPS_FEATURE)
        return "video/x-raw(" VASURFACE_FEATURE_STR "), format=" WATERMARK_PREFERRED_REMOTE_FORMAT;
    if (mem_type == DMA_BUF_CAPS_FEATURE)
        return "video/x-raw(" DMABUF_FEATURE_STR "), format=" WATERMARK_PREFERRED_REMOTE_FORMAT;
    // Only VASurface and DMABuf are supported!
    g_assert(FALSE);
    return "";
}

// VA-API path:
// |ghost sink| -> <identity> -> <vaapipostproc> -> <capsfilter> -> <watermarkimpl> -> |ghost src|
static gboolean gva_watermark_use_vaapi_path(GstGvaWatermark *self, CapsFeature mem_type) {
    g_assert(self->active_path != WatermarkPathVaapi);
    g_assert(mem_type == VA_SURFACE_CAPS_FEATURE || mem_type == DMA_BUF_CAPS_FEATURE);

    GstElement *preproc = gst_element_factory_make("vaapipostproc", "preproc");
    GstElement *postproc = gst_element_factory_make("vaapipostproc", "postproc");
    if (!preproc || !postproc) {
        GST_ELEMENT_ERROR(self, CORE, MISSING_PLUGIN, ("GStreamer installation is missing plugin vaapipostproc"),
                          (NULL));
        return FALSE;
    }

    GstElement *capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    gst_util_set_object_arg(G_OBJECT(capsfilter), "caps", get_caps_str_with_feature(mem_type));

    gst_bin_add_many(GST_BIN(self), preproc, postproc, capsfilter, NULL);

    // Save elements, so can easily remove them later
    self->vaapi_path_elems[0] = preproc;
    self->vaapi_path_elems[1] = capsfilter;
    self->vaapi_path_elems[2] = postproc;

    if (!gst_bin_sync_children_states(GST_BIN(self))) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't sync elements state with parent bin"), (NULL));
        return FALSE;
    }

    gboolean ret = gva_watermark_link_elements(self, self->identity, preproc) &&
                   gva_watermark_link_elements(self, preproc, capsfilter) &&
                   gva_watermark_link_elements(self, capsfilter, self->watermarkimpl) &&
                   gva_watermark_link_elements(self, self->watermarkimpl, postproc);

    if (!ret)
        return FALSE;

    if (!gva_watermark_set_src_pad(self, postproc))
        return FALSE;

    self->active_path = WatermarkPathVaapi;
    return TRUE;
}

// Direct path:
// |ghost sink| -> <identity> -> <watermarkimpl> -> |ghost src|
static gboolean gva_watermark_use_direct_path(GstGvaWatermark *self) {
    g_assert(self->active_path != WatermarkPathDirect);

    if (!gva_watermark_link_elements(self, self->identity, self->watermarkimpl))
        return FALSE;

    if (!gva_watermark_set_src_pad(self, self->watermarkimpl))
        return FALSE;

    self->active_path = WatermarkPathDirect;
    return TRUE;
}

void gva_watermark_unlink_and_remove_elements(GstGvaWatermark *self) {
    gst_element_unlink(self->identity, self->watermarkimpl);

    for (guint i = 0; i < sizeof(self->vaapi_path_elems) / sizeof(self->vaapi_path_elems[0]); i++) {
        if (self->vaapi_path_elems[i]) {
            gst_element_set_state(self->vaapi_path_elems[i], GST_STATE_NULL);
            gst_bin_remove(GST_BIN(self), self->vaapi_path_elems[i]);
            self->vaapi_path_elems[i] = NULL;
        }
    }
}

static gboolean gva_watermark_switch_path(GstGvaWatermark *self, enum WatermarkPath path, CapsFeature mem_type) {
    g_assert(path != WatermarkPathNone);

    GST_DEBUG_OBJECT(self, "Switching to path: %d, memory type: %d", path, mem_type);

    // Check if we already using the requested path
    if (self->active_path == path)
        return TRUE;

    // Block incoming data
    gva_watermark_block_sink(self, TRUE);

    if (self->active_path != WatermarkPathNone) {
        // Unlink proxy src pad
        gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->srcpad), NULL);

        // Clean-up current path
        gva_watermark_unlink_and_remove_elements(self);
    }

    gboolean result = FALSE;
    switch (path) {
    case WatermarkPathDirect:
        result = gva_watermark_use_direct_path(self);
        break;

    case WatermarkPathVaapi:
        result = gva_watermark_use_vaapi_path(self, mem_type);
        break;

    case WatermarkPathNone:
    default:
        break;
    }

    // Remove block
    gva_watermark_block_sink(self, FALSE);

    return result;
}

static gboolean gst_gva_watermark_sink_event(GstPad *pad, GstObject *parent, GstEvent *event) {
    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_DEBUG_OBJECT(parent, "Got CAPS event, caps: %" GST_PTR_FORMAT, caps);

        enum WatermarkPath path = WatermarkPathDirect;
        CapsFeature memory_type = get_caps_feature(caps);
        if (memory_type == VA_SURFACE_CAPS_FEATURE || memory_type == DMA_BUF_CAPS_FEATURE)
            path = WatermarkPathVaapi;

        GstGvaWatermark *self = GST_GVA_WATERMARK(parent);
        if (!gva_watermark_switch_path(self, path, memory_type)) {
            gst_event_unref(event);
            return FALSE;
        }
    }

    return gst_pad_event_default(pad, parent, event);
}

static gboolean gva_watermark_start(GstGvaWatermark *self) {
    if (!self->watermarkimpl || !self->identity)
        return FALSE;

    // Set ghost pad to input identity element
    GstPad *pad = gst_element_get_static_pad(self->identity, "sink");
    gboolean ret = gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->sinkpad), pad);
    gst_object_unref(pad);

    if (!ret)
        return FALSE;

    return ret;
}

static GstStateChangeReturn gva_watermark_change_state(GstElement *element, GstStateChange transition) {
    GstGvaWatermark *self = GST_GVA_WATERMARK(element);

    if (transition == GST_STATE_CHANGE_NULL_TO_READY) {
        if (!gva_watermark_start(self))
            return GST_STATE_CHANGE_FAILURE;
    }

    return GST_ELEMENT_CLASS(gst_gva_watermark_parent_class)->change_state(element, transition);
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvawatermark, "DL Streamer gvawatermark element", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
