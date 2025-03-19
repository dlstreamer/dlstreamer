/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvadrop.h"

#include <gst/gstevent.h>

#include <stdexcept>
#include <string>

GST_DEBUG_CATEGORY_STATIC(gva_drop_debug_category);
#define GST_CAT_DEFAULT gva_drop_debug_category

namespace {

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);
GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

constexpr auto ELEMENT_LONG_NAME = "Pass / drop custom number of frames in pipeline";
constexpr auto ELEMENT_DESCRIPTION = "Pass / drop custom number of frames in pipeline";

constexpr guint MIN_PASS_FRAMES = 1;
constexpr guint MAX_PASS_FRAMES = G_MAXUINT;
constexpr guint DEFAULT_PASS_FRAMES = 1;

constexpr guint MIN_DROP_FRAMES = 0;
constexpr guint MAX_DROP_FRAMES = G_MAXUINT;
constexpr guint DEFAULT_DROP_FRAMES = 0;

constexpr auto DEFAULT_MODE = DropMode::DEFAULT;
// Enum value names
constexpr auto UNKNOWN_VALUE_NAME = "unknown";
constexpr auto MODE_DEFAULT_NAME = "default";
constexpr auto MODE_GAP_EVENT_NAME = "gap";

enum { PROP_0, PROP_PASS_FRAMES, PROP_DROP_FRAMES, PROP_MODE };

std::string mode_to_string(DropMode mode) {
    switch (mode) {
    case DropMode::DEFAULT:
        return MODE_DEFAULT_NAME;
    case DropMode::GAP_EVENT:
        return MODE_GAP_EVENT_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

GstFlowReturn mode_handle(GvaDrop *self, GstBuffer *buffer) {
    switch (self->mode) {
    case DropMode::DEFAULT: {
        GST_DEBUG_OBJECT(self, "Drop buffer: frame=%u ts=%" GST_TIME_FORMAT, self->frames_counter,
                         GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }
    case DropMode::GAP_EVENT: {
        auto event = gst_event_new_gap(GST_BUFFER_PTS(buffer), GST_BUFFER_DURATION(buffer));

        GST_DEBUG_OBJECT(self, "Push GAP event: frame=%u ts=%" GST_TIME_FORMAT, self->frames_counter,
                         GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
        if (!gst_pad_push_event(GST_BASE_TRANSFORM(self)->srcpad, event)) {
            GST_ERROR_OBJECT(self, "Failed to push GAP event");
            return GST_FLOW_ERROR;
        }
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }
    default:
        throw std::runtime_error("Unknown drop type");
    }
}

} // namespace

/* class initialization */

G_DEFINE_TYPE(GvaDrop, gva_drop, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GVA_DROP_MODE (gva_drop_mode_get_type())
static GType gva_drop_mode_get_type(void) {
    static const GEnumValue modes[] = {{DropMode::DEFAULT, "Default", MODE_DEFAULT_NAME},
                                       {DropMode::GAP_EVENT, "Gap", MODE_GAP_EVENT_NAME},
                                       {0, NULL, NULL}};

    static GType gva_drop_mode = g_enum_register_static("GvaDropMode", modes);
    return gva_drop_mode;
}

static void gva_drop_reset(GvaDrop *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    self->pass_frames = DEFAULT_PASS_FRAMES;
    self->drop_frames = DEFAULT_DROP_FRAMES;
    self->frames_counter = 0;
}

static void gva_drop_init(GvaDrop *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
    gva_drop_reset(self);
}

static gboolean gva_drop_start(GstBaseTransform *trans) {
    GvaDrop *self = GVA_DROP(trans);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters: -- Pass frames: %d\n -- Drop frames: %d\n -- Mode: %s\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)), self->pass_frames, self->drop_frames,
                    mode_to_string(self->mode).c_str());

    return TRUE;
}

void gva_drop_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GvaDrop *self = GVA_DROP(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (property_id) {
    case PROP_PASS_FRAMES:
        self->pass_frames = g_value_get_uint(value);
        break;
    case PROP_DROP_FRAMES:
        self->drop_frames = g_value_get_uint(value);
        break;
    case PROP_MODE:
        self->mode = static_cast<DropMode>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gva_drop_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GvaDrop *self = GVA_DROP(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (property_id) {
    case PROP_PASS_FRAMES:
        g_value_set_uint(value, self->pass_frames);
        break;
    case PROP_DROP_FRAMES:
        g_value_set_uint(value, self->drop_frames);
        break;
    case PROP_MODE:
        g_value_set_enum(value, self->mode);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static GstFlowReturn gva_drop_transform_ip(GstBaseTransform *trans, GstBuffer *buffer) {
    GvaDrop *self = GVA_DROP(trans);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->drop_frames == 0)
        return GST_FLOW_OK;

    self->frames_counter++;
    if (self->frames_counter > self->pass_frames) {
        if (self->frames_counter == self->pass_frames + self->drop_frames) {
            self->frames_counter = 0;
        }
        return mode_handle(self, buffer);
    }

    GST_DEBUG_OBJECT(self, "Pass buffer: frame=%u ts=%" GST_TIME_FORMAT, self->frames_counter,
                     GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
    return GST_FLOW_OK;
}

static void gva_drop_class_init(GvaDropClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sinktemplate));
    gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&srctemplate));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gva_drop_set_property;
    gobject_class->get_property = gva_drop_get_property;
    base_transform_class->start = GST_DEBUG_FUNCPTR(gva_drop_start);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gva_drop_transform_ip);

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(gobject_class, PROP_PASS_FRAMES,
                                    g_param_spec_uint("pass-frames", "Pass Frames",
                                                      "Number of frames to pass along the pipeline.", MIN_PASS_FRAMES,
                                                      MAX_PASS_FRAMES, DEFAULT_PASS_FRAMES, prm_flags));

    g_object_class_install_property(gobject_class, PROP_DROP_FRAMES,
                                    g_param_spec_uint("drop-frames", "Drop Frames", "Number of frames to drop.",
                                                      MIN_DROP_FRAMES, MAX_DROP_FRAMES, DEFAULT_DROP_FRAMES,
                                                      prm_flags));
    g_object_class_install_property(gobject_class, PROP_MODE,
                                    g_param_spec_enum("mode", "Drop mode",
                                                      "Mode defines what to do with dropped frames",
                                                      GST_TYPE_GVA_DROP_MODE, DEFAULT_MODE, prm_flags));
}
