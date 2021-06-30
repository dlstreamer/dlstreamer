/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensoracc.hpp"
#include "converters/sliding_window_accumulator.hpp"

#include <capabilities/capabilities.hpp>
#include <utils.h>

#include <stdexcept>

GST_DEBUG_CATEGORY(gst_gva_tensor_acc_debug_category);

G_DEFINE_TYPE(GstGvaTensorAcc, gst_gva_tensor_acc, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GVA_TENSOR_ACC_MODE (gst_gva_tensor_acc_mode_get_type())
static GType gst_gva_tensor_acc_mode_get_type(void) {
    static GType gva_tensor_acc_mode_type = 0;
    static const GEnumValue mode_types[] = {{MODE_SLIDING_WINDOW, "Sliding window", "sliding-window"}, {0, NULL, NULL}};

    if (!gva_tensor_acc_mode_type) {
        gva_tensor_acc_mode_type = g_enum_register_static("GstGVATensorAccMode", mode_types);
    }

    return gva_tensor_acc_mode_type;
}

// TODO: define max and min values for size/step
static constexpr auto DEFAULT_ACC_MODE = MODE_SLIDING_WINDOW;
static constexpr auto MAX_WINDOW_STEP = UINT_MAX;
static constexpr auto MIN_WINDOW_STEP = 1;
static constexpr auto DEFAULT_SLIDE_WINDOW_STEP = 1;
static constexpr auto MAX_WINDOW_SIZE = UINT_MAX;
static constexpr auto MIN_WINDOW_SIZE = 1;
static constexpr auto DEFAULT_SLIDE_WINDOW_SIZE = 16;

enum { PROP_0, PROP_ACC_MODE, PROP_WINDOW_STEP, PROP_WINDOW_SIZE };

static std::unique_ptr<IAccumulator> create_accumulator(GstGvaTensorAcc *gvatensoracc) {
    g_assert(gvatensoracc && "GvaTensorAcc instance is null");

    try {
        switch (gvatensoracc->props.mode) {
        case MODE_SLIDING_WINDOW:
            return std::unique_ptr<SlidingWindowAccumulator>(
                new SlidingWindowAccumulator(gvatensoracc->props.window_size, gvatensoracc->props.window_step));
        default:
            throw std::runtime_error("Unsupported accumulation mode");
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(gvatensoracc, "Error while creating accumulator instance: %s",
                         Utils::createNestedErrorMsg(e).c_str());
    }

    return nullptr;
}

static void gst_gva_tensor_acc_cleanup(GstGvaTensorAcc *gvatensoracc) {
    if (!gvatensoracc)
        return;

    gvatensoracc->props.accumulator.reset();
}

static void gst_gva_tensor_acc_reset(GstGvaTensorAcc *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (!self)
        return;

    gst_gva_tensor_acc_cleanup(self);

    self->props.mode = MODE_SLIDING_WINDOW;
    self->props.window_size = DEFAULT_SLIDE_WINDOW_SIZE;
    self->props.window_step = DEFAULT_SLIDE_WINDOW_STEP;
}

static void gst_gva_tensor_acc_init(GstGvaTensorAcc *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GstGvaTensorAcc::_Props();

    gst_gva_tensor_acc_reset(self);
}

static void gst_gva_tensor_acc_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_ACC_MODE:
        self->props.mode = static_cast<AccumulateMode>(g_value_get_enum(value));
        break;
    case PROP_WINDOW_STEP:
        self->props.window_step = g_value_get_uint(value);
        break;
    case PROP_WINDOW_SIZE:
        self->props.window_size = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_tensor_acc_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_ACC_MODE:
        g_value_set_enum(value, self->props.mode);
        break;
    case PROP_WINDOW_STEP:
        g_value_set_uint(value, self->props.window_step);
        break;
    case PROP_WINDOW_SIZE:
        g_value_set_uint(value, self->props.window_size);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_tensor_acc_dispose(GObject *object) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gst_gva_tensor_acc_parent_class)->dispose(object);
}

static void gst_gva_tensor_acc_finalize(GObject *object) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    gst_gva_tensor_acc_cleanup(self);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gst_gva_tensor_acc_parent_class)->finalize(object);
}

static gboolean gst_gva_tensor_acc_set_caps(GstBaseTransform *base, GstCaps * /*incaps*/, GstCaps * /*outcaps*/) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gst_gva_tensor_acc_sink_event(GstBaseTransform *base, GstEvent *event) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // TODO: push buffer with result on EOS or Flush?

    return GST_BASE_TRANSFORM_CLASS(gst_gva_tensor_acc_parent_class)->sink_event(base, event);
}

static gboolean gst_gva_tensor_acc_start(GstBaseTransform *base) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    self->props.accumulator = create_accumulator(self);
    if (!self->props.accumulator) {
        GST_ERROR_OBJECT(self, "Failed to create accumulator instance");
        return false;
    }

    return true;
}

static gboolean gst_gva_tensor_acc_stop(GstBaseTransform *base) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gst_gva_tensor_acc_transform_size(GstBaseTransform *trans, GstPadDirection direction,
                                                  GstCaps * /*caps*/, gsize /*size*/, GstCaps * /*othercaps*/,
                                                  gsize *othersize) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(trans);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* GStreamer hardcoded call with GST_PAD_SINK only */
    g_assert(direction == GST_PAD_SINK);
    *othersize = 0;

    return true;
}

GstCaps *gst_gva_tensor_acc_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps,
                                           GstCaps *filter) {
    GstCaps *srccaps, *sinkcaps;
    GstCaps *ret = NULL;

    srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans));
    sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans));

    switch (direction) {
    case GST_PAD_SINK:
        if (gst_caps_can_intersect(caps, sinkcaps))
            ret = gst_caps_copy(srccaps);
        else
            ret = gst_caps_new_empty();
        break;
    case GST_PAD_SRC:
        if (gst_caps_can_intersect(caps, srccaps))
            ret = gst_caps_copy(sinkcaps);
        else
            ret = gst_caps_new_empty();
        break;
    default:
        g_assert_not_reached();
    }

    GST_DEBUG_OBJECT(trans, "transformed %" GST_PTR_FORMAT, ret);

    if (filter) {
        GstCaps *intersection;

        GST_DEBUG_OBJECT(trans, "Using filter caps %" GST_PTR_FORMAT, filter);

        intersection = gst_caps_intersect_full(filter, ret, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(ret);
        ret = intersection;

        GST_DEBUG_OBJECT(trans, "Intersection %" GST_PTR_FORMAT, ret);
    }

    gst_caps_unref(srccaps);
    gst_caps_unref(sinkcaps);

    return ret;
}

static GstFlowReturn gst_gva_tensor_acc_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GstGvaTensorAcc *self = GST_GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* TODO: is there any way we wil receive more than one memory ? */
    g_assert(gst_buffer_n_memory(inbuf) == 1);
    g_assert(gst_buffer_n_memory(outbuf) == 0);

    GstMemory *mem = gst_buffer_peek_memory(inbuf, 0);
    self->props.accumulator->accumulate(mem);
    auto acc_result = self->props.accumulator->get_result();
    if (acc_result == nullptr) {
        GST_ERROR_OBJECT(self, "Failed to create an accumulation result");
        // TODO: copy mem from inbuf or what should be done if we couldn't accumulate?
        return GST_FLOW_OK;
    }
    gst_buffer_append_memory(outbuf, acc_result);

    return GST_FLOW_OK;
}

static void gst_gva_tensor_acc_class_init(GstGvaTensorAccClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_tensor_acc_set_property;
    gobject_class->get_property = gst_gva_tensor_acc_get_property;
    gobject_class->dispose = gst_gva_tensor_acc_dispose;
    gobject_class->finalize = gst_gva_tensor_acc_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gst_gva_tensor_acc_set_caps;
    base_transform_class->sink_event = gst_gva_tensor_acc_sink_event;
    base_transform_class->start = gst_gva_tensor_acc_start;
    base_transform_class->stop = gst_gva_tensor_acc_stop;
    base_transform_class->transform = gst_gva_tensor_acc_transform;
    base_transform_class->transform_caps = gst_gva_tensor_acc_transform_caps;
    base_transform_class->transform_size = gst_gva_tensor_acc_transform_size;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_ACC_NAME, "application", GVA_TENSOR_ACC_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_TENSOR_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSOR_CAPS)));

    g_object_class_install_property(gobject_class, PROP_ACC_MODE,
                                    g_param_spec_enum("mode", "Accumulation mode", "Mode to accumulate tensors",
                                                      GST_TYPE_GVA_TENSOR_ACC_MODE, DEFAULT_ACC_MODE,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_WINDOW_STEP,
                                    g_param_spec_uint("window-step", "Window step", "Sliding window step",
                                                      MIN_WINDOW_STEP, MAX_WINDOW_STEP, DEFAULT_SLIDE_WINDOW_STEP,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_WINDOW_SIZE,
                                    g_param_spec_uint("window-size", "Window size", "Sliding window size",
                                                      MIN_WINDOW_SIZE, MAX_WINDOW_SIZE, DEFAULT_SLIDE_WINDOW_SIZE,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
