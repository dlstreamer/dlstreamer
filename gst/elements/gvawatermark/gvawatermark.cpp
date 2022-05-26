/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvawatermark.h"
#include "gvawatermarkcaps.h"

#include "scope_guard.h"

#include <gst/video/video.h>

#include <string>

#define ELEMENT_LONG_NAME "Bin element for detection/classification/recognition results labeling"
#define ELEMENT_DESCRIPTION "Overlays the metadata on the video frame to visualize the inference results."

GST_DEBUG_CATEGORY_STATIC(gst_gva_watermark_debug_category);
#define GST_CAT_DEFAULT gst_gva_watermark_debug_category

#define DEFAULT_DEVICE nullptr

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
static GstStateChangeReturn gva_watermark_change_state(GstElement *element, GstStateChange transition);
static gboolean gst_gva_watermark_sink_event(GstPad *pad, GstObject *parent, GstEvent *event);

namespace {

static const gchar *get_caps_str_with_feature(CapsFeature mem_type) {
    if (mem_type == VA_SURFACE_CAPS_FEATURE)
        return "video/x-raw(" VASURFACE_FEATURE_STR "), format=" WATERMARK_PREFERRED_REMOTE_FORMAT;
    if (mem_type == DMA_BUF_CAPS_FEATURE)
        return "video/x-raw(" DMABUF_FEATURE_STR "), format=" WATERMARK_PREFERRED_REMOTE_FORMAT;
    if (mem_type == SYSTEM_MEMORY_CAPS_FEATURE)
        return "video/x-raw";
    g_assert(FALSE && "Only VASurface, DMABuf, and System memory are supported !");
    return "";
}

static bool is_caps_format_equal(GstCaps *caps, const std::string &format) {
    bool result = true;

    auto size = gst_caps_get_size(caps);
    for (size_t i = 0; i < size; ++i) {
        auto structure = gst_caps_get_structure(caps, i);
        auto str = gst_structure_get_string(structure, "format");
        if (str == nullptr || str != format) {
            return false;
        }
    }

    return result;
}

} /* anonymous namespace */

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

    gst_element_add_pad(GST_ELEMENT(self), self->sinkpad);
    gst_element_add_pad(GST_ELEMENT(self), self->srcpad);

    gst_pad_set_event_function(self->sinkpad, GST_DEBUG_FUNCPTR(gst_gva_watermark_sink_event));

    self->identity = gst_element_factory_make("identity", nullptr);
    if (!self->identity)
        GST_ERROR_OBJECT(self, "Could not create identity instance");

    self->watermarkimpl = gst_element_factory_make("gvawatermarkimpl", nullptr);
    if (!self->watermarkimpl)
        GST_ERROR_OBJECT(self, "Could not create gvawatermark instance");

    gst_bin_add_many(GST_BIN(self), GST_ELEMENT(gst_object_ref(self->identity)),
                     GST_ELEMENT(gst_object_ref(self->watermarkimpl)), nullptr);

    auto factory = gst_element_factory_find("vaapipostproc");
    auto sg_factory = makeScopeGuard([factory]() {
        if (factory)
            gst_object_unref(factory);
    });

#ifdef ENABLE_VAAPI
    self->have_vaapi = factory != nullptr;
#else
    self->have_vaapi = false;
#endif

    self->active_path = WatermarkPathNone;
    self->preferred_path = WatermarkPathNone;
    self->is_active_nv12 = false;
    self->device = DEFAULT_DEVICE;
    self->block_probe_id = 0;
}

void gst_gva_watermark_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "set_property");

    switch (property_id) {
    case PROP_DEVICE:
        g_free(gvawatermark->device);
        gvawatermark->device = g_value_dup_string(value);
        g_object_set(gvawatermark->watermarkimpl, "device", gvawatermark->device, nullptr);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_watermark_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "get_property");

    switch (property_id) {
    case PROP_DEVICE:
        g_value_set_string(value, gvawatermark->device);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_watermark_finalize(GObject *object) {
    GstGvaWatermark *gvawatermark = GST_GVA_WATERMARK(object);

    GST_DEBUG_OBJECT(gvawatermark, "finalize");

    if (gvawatermark->watermarkimpl) {
        gst_object_unref(gvawatermark->watermarkimpl);
        gvawatermark->watermarkimpl = nullptr;
    }

    g_free(gvawatermark->device);
    gvawatermark->device = nullptr;

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
        self->block_probe_id = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
                                                 gva_watermark_sink_block_pad_probe, nullptr, nullptr);
    } else {
        gst_pad_remove_probe(pad, self->block_probe_id);
        self->block_probe_id = 0;
    }

    gst_object_unref(pad);
    GST_DEBUG_OBJECT(self, "Sink block set to: %d", enable_block);
}

static gboolean gva_watermark_link_elements(GstGvaWatermark *self, GstElement *src, GstElement *dest) {
    if (!gst_element_link(src, dest)) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND,
                          ("Couldn't link element %s to %s", GST_ELEMENT_NAME(src), GST_ELEMENT_NAME(dest)), (nullptr));
        return FALSE;
    }
    return TRUE;
}

static gboolean gva_watermark_set_src_pad(GstGvaWatermark *self, GstElement *src) {
    GstPad *pad = gst_element_get_static_pad(src, "src");
    gboolean ret = gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->srcpad), pad);
    gst_object_unref(pad);
    return ret;
}

CapsFeature get_current_caps_feature(GstGvaWatermark *self) {
    if (!self->capsfilter) {
        return ANY_CAPS_FEATURE;
    }

    GstCaps *caps = nullptr;
    g_object_get(G_OBJECT(self->capsfilter), "caps", &caps, nullptr);
    if (caps == nullptr) {
        return ANY_CAPS_FEATURE;
    }
    return get_caps_feature(caps);
}

static gboolean link_videoconvert(GstGvaWatermark *self) {
    g_assert(self->active_path == WatermarkPathVaapi && "Supposed to be called in VA-API path");

    self->convert = gst_element_factory_make("videoconvert", nullptr);
    if (!self->convert) {
        GST_ELEMENT_ERROR(self, CORE, MISSING_PLUGIN, ("GStreamer installation is missing plugin videoconvert"),
                          (nullptr));
        return FALSE;
    }
    gst_bin_add(GST_BIN(self), self->convert);
    if (!gst_element_sync_state_with_parent(self->convert)) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't sync videoconvert state with gvawatermark"), (nullptr));
        return FALSE;
    }

    gst_element_unlink(self->identity, self->preproc);
    if (!gst_element_link(self->identity, self->convert) || !gst_element_link(self->convert, self->preproc)) {
        GST_ERROR_OBJECT(self, "videoconvert cannot be linked");
        return FALSE;
    }

    return TRUE;
}

static gboolean unlink_videoconvert(GstGvaWatermark *self) {
    if (!self->convert || self->active_path != WatermarkPathVaapi)
        return TRUE;

    gst_element_unlink(self->identity, self->convert);
    gst_element_unlink(self->convert, self->preproc);
    if (!gst_element_link(self->identity, self->preproc)) {
        GST_ERROR_OBJECT(self, "Unable to link identity to vaapipostproc after removing videoconvert");
        return FALSE;
    }

    gst_element_set_state(self->convert, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(self), self->convert);
    self->convert = nullptr;

    return TRUE;
}

// VA-API path:
// |ghost sink| -> <identity> -> <vaapipostproc> -> <capsfilter> -> <watermarkimpl> -> <vaapipostproc> -> |ghost src|
static gboolean gva_watermark_link_vaapi_path(GstGvaWatermark *self, CapsFeature in_mem_type) {
    self->preproc = gst_element_factory_make("vaapipostproc", nullptr);
    self->capsfilter = gst_element_factory_make("capsfilter", nullptr);
    self->postproc = gst_element_factory_make("vaapipostproc", nullptr);

    if (!self->preproc || !self->postproc || !self->capsfilter) {
        GST_ELEMENT_ERROR(self, CORE, MISSING_PLUGIN, ("GStreamer installation is missing plugins of VA-API path"),
                          (nullptr));
        return FALSE;
    }

    gst_bin_add_many(GST_BIN(self), self->preproc, self->postproc, self->capsfilter, nullptr);

    gst_util_set_object_arg(G_OBJECT(self->capsfilter), "caps", get_caps_str_with_feature(in_mem_type));

    if (!gst_bin_sync_children_states(GST_BIN(self))) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't sync elements state with parent bin"), (nullptr));
        return FALSE;
    }

    if (!gst_element_link(self->identity, self->preproc)) {
        GST_INFO_OBJECT(self, "vaapipostproc cannot be linked, unsupported format");
        return FALSE;
    }

    gboolean ret = gva_watermark_link_elements(self, self->preproc, self->capsfilter) &&
                   gva_watermark_link_elements(self, self->capsfilter, self->watermarkimpl) &&
                   gva_watermark_link_elements(self, self->watermarkimpl, self->postproc);

    if (!gva_watermark_set_src_pad(self, self->postproc)) {
        GST_ERROR_OBJECT(self, "Couldn't set target for src ghost pad");
        return FALSE;
    }

    if (!ret)
        return FALSE;

    self->active_path = WatermarkPathVaapi;
    return TRUE;
}

static void gva_watermark_unlink_vaapi_path(GstGvaWatermark *self) {
    if (self->convert) {
        gst_element_unlink(self->identity, self->convert);
        gst_element_unlink(self->convert, self->preproc);
    } else {
        gst_element_unlink(self->identity, self->preproc);
    }
    gst_element_unlink(self->preproc, self->capsfilter);
    gst_element_unlink(self->capsfilter, self->watermarkimpl);
    gst_element_unlink(self->watermarkimpl, self->postproc);
    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->srcpad), NULL);

    if (self->convert) {
        gst_element_set_state(self->convert, GST_STATE_NULL);
    }
    gst_element_set_state(self->preproc, GST_STATE_NULL);
    gst_element_set_state(self->capsfilter, GST_STATE_NULL);
    gst_element_set_state(self->postproc, GST_STATE_NULL);

    gst_bin_remove_many(GST_BIN(self), self->preproc, self->capsfilter, self->postproc, nullptr);

    if (self->convert) {
        gst_bin_remove(GST_BIN(self), self->convert);
    }

    self->convert = nullptr;
    self->preproc = nullptr;
    self->capsfilter = nullptr;
    self->postproc = nullptr;
}

// Direct path:
// |ghost sink| -> <identity> -> <watermarkimpl> -> |ghost src|
static gboolean gva_watermark_link_direct_path(GstGvaWatermark *self, bool use_postproc = false) {
    if (use_postproc) {
        self->postproc = gst_element_factory_make("vaapipostproc", nullptr);
        if (!self->postproc) {
            GST_ERROR_OBJECT(self, "Could not create vaapipostproc instance");
        }

        gst_bin_add(GST_BIN(self), self->postproc);
    }

    if (!gst_bin_sync_children_states(GST_BIN(self))) {
        GST_ELEMENT_ERROR(self, RESOURCE, NOT_FOUND, ("Couldn't sync elements state with parent bin"), (nullptr));
        return FALSE;
    }

    if (!gva_watermark_link_elements(self, self->identity, self->watermarkimpl))
        return FALSE;

    if (use_postproc && !gva_watermark_link_elements(self, self->watermarkimpl, self->postproc)) {
        return FALSE;
    }

    gboolean src_pad_is_set = (use_postproc) ? gva_watermark_set_src_pad(self, self->postproc)
                                             : gva_watermark_set_src_pad(self, self->watermarkimpl);

    if (!src_pad_is_set) {
        GST_ERROR_OBJECT(self, "Couldn't set target for src ghost pad");
        return FALSE;
    }

    self->active_path = WatermarkPathDirect;
    return TRUE;
}

static void gva_watermark_unlink_direct_path(GstGvaWatermark *self) {
    gst_element_unlink(self->identity, self->watermarkimpl);
    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->srcpad), NULL);

    if (self->postproc) {
        gst_element_unlink(self->watermarkimpl, self->postproc);
        gst_element_set_state(self->postproc, GST_STATE_NULL);
        gst_bin_remove(GST_BIN(self), self->postproc);
        self->postproc = nullptr;
    }
}

static gboolean gva_watermark_switch_path(GstGvaWatermark *self, enum WatermarkPath path, CapsFeature in_mem_type) {
    g_assert(path != WatermarkPathNone && "Cannot switch path to None");
    GST_DEBUG_OBJECT(self, "Switching to path: %d, memory type: %d", path, in_mem_type);

    // Check if we already using the requested path
    if (self->active_path == path)
        return TRUE;

    // Block incoming data
    gva_watermark_block_sink(self, TRUE);

    switch (self->active_path) {
    case WatermarkPathDirect:
        gva_watermark_unlink_direct_path(self);
        break;
    case WatermarkPathVaapi:
        gva_watermark_unlink_vaapi_path(self);
        break;
    case WatermarkPathNone:
        break;
    default:
        GST_ERROR_OBJECT(self, "Unexpected path received during the gvawatermark unlink");
        return FALSE;
    }

    gboolean result = FALSE;
    switch (path) {
    case WatermarkPathDirect:
        /* FIXME: using system caps with NV12 and VA-API elements after watermark, e.g. encoder,
         * lead to unexpected behavior. Remove when issue is resolved */
        result = gva_watermark_link_direct_path(self, self->is_active_nv12 && self->have_vaapi);
        break;
    case WatermarkPathVaapi:
        result = gva_watermark_link_vaapi_path(self, in_mem_type);
        break;
    case WatermarkPathNone:
    default:
        GST_ERROR_OBJECT(self, "Unexpected path received during the gvawatermark link");
        return FALSE;
    }

    // Remove block
    gva_watermark_block_sink(self, FALSE);

    return result;
}

static gboolean gst_gva_watermark_sink_event(GstPad *pad, GstObject *parent, GstEvent *event) {
    GstGvaWatermark *self = GST_GVA_WATERMARK(parent);

    switch (GST_EVENT_TYPE(event)) {
    case GST_EVENT_CAPS: {
        GstCaps *incaps;
        gst_event_parse_caps(event, &incaps);
        GST_DEBUG_OBJECT(parent, "Got CAPS event, caps: %" GST_PTR_FORMAT, incaps);

        auto target_memtype = get_caps_feature(incaps);

        /* FIXME: BGRx does not work with vaapipostproc
         * and in order to make such pipelines work
         * we put videoconvert between identity and vaapipostproc.
         * Relevant only for WatermarkPathVaapi
         */
        if (!is_caps_format_equal(incaps, "BGRx"))
            if (!unlink_videoconvert(self))
                return FALSE;

        self->is_active_nv12 = is_caps_format_equal(incaps, "NV12");

        /* save preferred path here to change on GST_EVENT_SEGMENT if needed */
        if (target_memtype == SYSTEM_MEMORY_CAPS_FEATURE)
            self->preferred_path = WatermarkPathDirect;
        else
            self->preferred_path = WatermarkPathVaapi;

    } break;
    case GST_EVENT_SEGMENT: {
        if (self->preferred_path == WatermarkPathDirect)
            if (!gva_watermark_switch_path(self, WatermarkPathDirect, SYSTEM_MEMORY_CAPS_FEATURE))
                return FALSE;
    } break;
    default:
        break;
    }

    return gst_pad_event_default(pad, parent, event);
}

static gboolean gva_watermark_start(GstGvaWatermark *self) {
    if (!self->watermarkimpl || !self->identity)
        return FALSE;

    // Set sink ghost pad to input identity element
    GstPad *pad = gst_element_get_static_pad(self->identity, "sink");
    gboolean ret = gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(self->sinkpad), pad);
    gst_object_unref(pad);

    if (!ret)
        return FALSE;

    if (self->have_vaapi) {
        if (gva_watermark_switch_path(self, WatermarkPathVaapi, VA_SURFACE_CAPS_FEATURE) && link_videoconvert(self))
            return TRUE;
        else
            GST_INFO_OBJECT(self, "Unsopported format on sink pad, switching to direct path");
    } else {
        GST_INFO_OBJECT(self, "vaapipostproc is not found, switching to direct path");
    }
    return gva_watermark_switch_path(self, WatermarkPathDirect, SYSTEM_MEMORY_CAPS_FEATURE);
}

static GstStateChangeReturn gva_watermark_change_state(GstElement *element, GstStateChange transition) {
    GstGvaWatermark *self = GST_GVA_WATERMARK(element);

    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY: {
        if (!gva_watermark_start(self))
            return GST_STATE_CHANGE_FAILURE;
    } break;
    default:
        break;
    }

    return GST_ELEMENT_CLASS(gst_gva_watermark_parent_class)->change_state(element, transition);
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvawatermark, PRODUCT_FULL_NAME " gvawatermark element",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
