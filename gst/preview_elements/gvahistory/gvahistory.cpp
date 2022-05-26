/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvahistory.hpp"
#include "algorithms/meta_history.hpp"

#include <capabilities/tensor_caps.hpp>
#include <gva_caps.h>
#include <meta/gva_buffer_flags.hpp>
#include <tensor_layer_desc.hpp>

#include <gst/gstevent.h>

#include <stdexcept>

GST_DEBUG_CATEGORY(gva_history_debug_category);
#define GST_CAT_DEFAULT gva_history_debug_category

namespace {

constexpr guint DEFAULT_MIN_INTERVAL = 0;
constexpr guint DEFAULT_MAX_INTERVAL = UINT_MAX;
constexpr guint DEFAULT_INTERVAL = DEFAULT_MIN_INTERVAL;

constexpr auto DEFAULT_TYPE = HistoryType::META_HISTORY;

// Enum value names
constexpr auto UNKNOWN_VALUE_NAME = "unknown";
constexpr auto TYPE_META_HISTORY_NAME = "meta";

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(GVA_CAPS));

std::string type_to_string(HistoryType type) {
    switch (type) {
    case HistoryType::META_HISTORY:
        return TYPE_META_HISTORY_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

enum { PROP_0, PROP_TYPE, PROP_INTERVAL };

std::unique_ptr<IHistory> create_history(GvaHistory *gvahistory) {
    g_assert(gvahistory && "GvaHistory instance is null");

    try {
        switch (gvahistory->props.type) {
        case HistoryType::META_HISTORY:
            return std::unique_ptr<MetaHistory>(new MetaHistory(gvahistory->props.interval));
        default:
            throw std::runtime_error("Unsupported history type");
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(gvahistory, "Error while creating history instance: %s",
                         Utils::createNestedErrorMsg(e).c_str());
    }

    return nullptr;
}

GstPad *query_postproc_srcpad(GvaHistory *self) {
    auto query = gva_query_new_postproc_srcpad();
    if (!gst_pad_peer_query(GST_BASE_TRANSFORM(self)->srcpad, query)) {
        gst_query_unref(query);
        return nullptr;
    }
    GstPad *postproc_srcpad = nullptr;
    gva_query_parse_postproc_srcpad(query, postproc_srcpad);
    gst_query_unref(query);
    return postproc_srcpad;
}

GstPadProbeReturn metahistory_buffer_probe(GstPad *, GstPadProbeInfo *info, gpointer history) {
    g_assert(history && "Expected MetaHistory to be passed as user data to buffer probe");

    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer) {
        GST_DEBUG("Got buffer on pad probe: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        try {
            auto meta_history = static_cast<MetaHistory *>(history);
            meta_history->save(buffer);
        } catch (const std::exception &e) {
            GST_ERROR("Error while trying to save results from buffer: %s", Utils::createNestedErrorMsg(e).c_str());
        }
    } else {
        GST_ERROR("Invalid data received from buffer probe callback");
    }
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn metahistory_event_probe(GstPad *pad, GstPadProbeInfo *info, gpointer history) {
    g_assert(history && "Expected MetaHistory to be passed as user data to event probe");

    auto event = GST_PAD_PROBE_INFO_EVENT(info);
    if (!event || event->type != GST_EVENT_GAP)
        return GST_PAD_PROBE_OK;

    GstBuffer *gapbuf = nullptr;
    if (!gva_event_parse_gap_with_buffer(event, &gapbuf) || !gapbuf)
        return GST_PAD_PROBE_OK;

    GST_DEBUG("Got GAP event with buffer on pad probe: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(gapbuf)));
    // TODO: we use GAP event to bypass inference and catch it on postproc src pad (here)
    // for meta_aggregate (and any other elements after postproc) we should provide buffer again
    // so here we get buffer from event we sent and push it in postproc src pad
    gst_event_unref(event);

    try {
        auto meta_history = static_cast<MetaHistory *>(history);
        meta_history->fill(gapbuf);
    } catch (const std::exception &e) {
        GST_ERROR("Error while trying to fill buffer with saved results: %s", Utils::createNestedErrorMsg(e).c_str());
    }

    gst_pad_push(pad, gapbuf);
    return GST_PAD_PROBE_HANDLED;
}

} // namespace

G_DEFINE_TYPE(GvaHistory, gva_history, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GVA_HISTORY_TYPE (gva_history_type_get_type())
static GType gva_history_type_get_type(void) {
    static const GEnumValue types[] = {{HistoryType::META_HISTORY, "Meta", TYPE_META_HISTORY_NAME}, {0, NULL, NULL}};
    static GType gva_history_type = g_enum_register_static("GvaHistoryMode", types);
    return gva_history_type;
}

static void gva_history_init(GvaHistory *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with placement new
    new (&self->props) GvaHistory::_Props();
}

static void gva_history_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaHistory *self = GVA_HISTORY(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_TYPE:
        self->props.type = static_cast<HistoryType>(g_value_get_enum(value));
        break;
    case PROP_INTERVAL:
        self->props.interval = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_history_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaHistory *self = GVA_HISTORY(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_TYPE:
        g_value_set_enum(value, self->props.type);
        break;
    case PROP_INTERVAL:
        g_value_set_uint(value, self->props.interval);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_history_finalize(GObject *object) {
    GvaHistory *self = GVA_HISTORY(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_history_parent_class)->finalize(object);
}

static gboolean gva_history_set_caps(GstBaseTransform *base, GstCaps * /*incaps*/, GstCaps * /*outcaps*/) {
    GvaHistory *self = GVA_HISTORY(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->props.type == META_HISTORY) {
        GstPad *postproc_srcpad = query_postproc_srcpad(self);
        if (!postproc_srcpad) {
            GST_ERROR_OBJECT(self, "Failed to get src pad of post-processing element from query");
            return false;
        }
        gst_pad_add_probe(postproc_srcpad, GST_PAD_PROBE_TYPE_BUFFER, metahistory_buffer_probe,
                          self->props.processor.get(), NULL);
        gst_pad_add_probe(postproc_srcpad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, metahistory_event_probe,
                          self->props.processor.get(), NULL);
    }

    return true;
}

static gboolean gva_history_start(GstBaseTransform *base) {
    GvaHistory *self = GVA_HISTORY(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters:\n -- Type: %s\n -- Interval: %d\n", GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)),
                    type_to_string(self->props.type).c_str(), self->props.interval);

    self->props.processor = create_history(self);
    if (!self->props.processor) {
        GST_ERROR_OBJECT(self, "Failed to create history instance");
        return false;
    }

    return true;
}

static GstFlowReturn gva_history_transform_ip(GstBaseTransform *base, GstBuffer *buf) {
    GvaHistory *self = GVA_HISTORY(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    bool skip = false;

    try {
        skip = self->props.processor->invoke(buf);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during history invoke: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    if (skip) {
        GST_DEBUG_OBJECT(self, "Pass buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
        return GST_FLOW_OK;
    }

    // TODO: looks ugly, think about it
    auto copy = gst_buffer_copy(buf);
    auto gap_event = gva_event_new_gap_with_buffer(copy);
    gst_buffer_unref(copy);

    GST_DEBUG_OBJECT(self, "Push GAP event and drop buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buf)));
    if (!gst_pad_push_event(base->srcpad, gap_event)) {
        GST_ERROR_OBJECT(self, "Failed to push gva gap event with buffer meta");
        return GST_FLOW_ERROR;
    }

    return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static void gva_history_class_init(GvaHistoryClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_history_set_property;
    gobject_class->get_property = gva_history_get_property;
    gobject_class->finalize = gva_history_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gva_history_set_caps;
    base_transform_class->start = gva_history_start;
    base_transform_class->transform_ip = gva_history_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_HISTORY_NAME, "application", GVA_HISTORY_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sinktemplate));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&srctemplate));

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(gobject_class, PROP_TYPE,
                                    g_param_spec_enum("type", "History type",
                                                      "Type defines which history algorithm to use.\n"
                                                      "Only valid when used in conjunction with gvatrack",
                                                      GST_TYPE_GVA_HISTORY_TYPE, DEFAULT_TYPE, prm_flags));
    g_object_class_install_property(
        gobject_class, PROP_INTERVAL,
        g_param_spec_uint("interval", "Interval", "Determines the frequency of running inference on tracked objects",
                          DEFAULT_MIN_INTERVAL, DEFAULT_MAX_INTERVAL, DEFAULT_INTERVAL, prm_flags));
}
