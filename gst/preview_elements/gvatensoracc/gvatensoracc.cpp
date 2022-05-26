/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensoracc.hpp"
#include "converters/condition_accumulator.hpp"
#include "converters/sliding_window_accumulator.hpp"

#include <capabilities/tensor_caps.hpp>
#include <tensor_layer_desc.hpp>
#include <utils.h>

#include <stdexcept>

namespace {

// TODO: define max and min values for size/step
constexpr auto DEFAULT_ACC_MODE = MODE_SLIDING_WINDOW;
constexpr auto MAX_WINDOW_STEP = UINT_MAX;
constexpr auto MIN_WINDOW_STEP = 1;
constexpr auto DEFAULT_SLIDE_WINDOW_STEP = 1;
constexpr auto MAX_WINDOW_SIZE = UINT_MAX;
constexpr auto MIN_WINDOW_SIZE = 1;
constexpr auto DEFAULT_SLIDE_WINDOW_SIZE = 16;
static constexpr auto DEFAULT_ACC_DATA = MEMORY;

// Enum value names
constexpr auto UNKNOWN_VALUE_NAME = "unknown";
constexpr auto MODE_SLIDING_WINDOW_NAME = "sliding-window";
constexpr auto MODE_CONDITION_NAME = "condition";

std::string mode_to_string(AccumulateMode mode) {
    switch (mode) {
    case MODE_SLIDING_WINDOW:
        return MODE_SLIDING_WINDOW_NAME;
    case MODE_CONDITION:
        return MODE_CONDITION_NAME;
    default:
        return UNKNOWN_VALUE_NAME;
    }
}

enum { PROP_0, PROP_ACC_MODE, PROP_WINDOW_STEP, PROP_WINDOW_SIZE, PROP_ACC_DATA };

// FIXME: class needed for CentOS
template <template <typename> class AccumulatorType, typename... Args>
static std::unique_ptr<IAccumulator> create_accumulator_by_data(AccumulateData data, Args &&... args) {
    switch (data) {
    case MEMORY:
        return std::unique_ptr<AccumulatorType<GstMemory>>(new AccumulatorType<GstMemory>(std::forward<Args>(args)...));
    case META:
        return std::unique_ptr<AccumulatorType<GstMeta>>(new AccumulatorType<GstMeta>(std::forward<Args>(args)...));
    default:
        throw std::runtime_error("Unsupported accumulation mode");
    }
}

std::unique_ptr<IAccumulator> create_accumulator(GvaTensorAcc *gvatensoracc) {
    g_assert(gvatensoracc && "GvaTensorAcc instance is null");

    try {
        switch (gvatensoracc->props.mode) {
        case MODE_SLIDING_WINDOW:
            return create_accumulator_by_data<SlidingWindowAccumulator>(
                gvatensoracc->props.data, gvatensoracc->props.window_size, gvatensoracc->props.window_step);
        case MODE_CONDITION:
            return create_accumulator_by_data<ConditionAccumulator>(gvatensoracc->props.data);
        default:
            throw std::runtime_error("Unsupported accumulation mode");
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(gvatensoracc, "Error while creating accumulator instance: %s",
                         Utils::createNestedErrorMsg(e).c_str());
    }

    return nullptr;
}
} // namespace

GST_DEBUG_CATEGORY(gva_tensor_acc_debug_category);
#define GST_CAT_DEFAULT gva_tensor_acc_debug_category

G_DEFINE_TYPE(GvaTensorAcc, gva_tensor_acc, GST_TYPE_BASE_TRANSFORM);

#define GST_TYPE_GVA_TENSOR_ACC_MODE (gva_tensor_acc_mode_get_type())
static GType gva_tensor_acc_mode_get_type(void) {
    static const GEnumValue mode_types[] = {{MODE_SLIDING_WINDOW, "Sliding window", MODE_SLIDING_WINDOW_NAME},
                                            {MODE_CONDITION, "Condition", MODE_CONDITION_NAME},
                                            {0, NULL, NULL}};

    static GType gva_tensor_acc_mode_type = g_enum_register_static("GvaTensorAccMode", mode_types);
    return gva_tensor_acc_mode_type;
}

#define GST_TYPE_GVA_TENSOR_ACC_DATA (gva_tensor_acc_data_get_type())
static GType gva_tensor_acc_data_get_type(void) {
    static const GEnumValue data_types[] = {{MEMORY, "Memory", "memory"}, {META, "Meta", "meta"}, {0, NULL, NULL}};
    static GType gva_tensor_acc_data_type = g_enum_register_static("GvaTensorAccData", data_types);
    return gva_tensor_acc_data_type;
}

static void gva_tensor_acc_cleanup(GvaTensorAcc *gvatensoracc) {
    if (!gvatensoracc)
        return;

    gvatensoracc->props.accumulator.reset();
}

static void gva_tensor_acc_reset(GvaTensorAcc *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (!self)
        return;

    gva_tensor_acc_cleanup(self);

    self->props.mode = MODE_SLIDING_WINDOW;
    self->props.window_size = DEFAULT_SLIDE_WINDOW_SIZE;
    self->props.window_step = DEFAULT_SLIDE_WINDOW_STEP;
}

static void gva_tensor_acc_init(GvaTensorAcc *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GvaTensorAcc::_Props();

    gva_tensor_acc_reset(self);
}

static void gva_tensor_acc_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(object);
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
    case PROP_ACC_DATA:
        self->props.data = static_cast<AccumulateData>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_tensor_acc_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(object);
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
    case PROP_ACC_DATA:
        g_value_set_enum(value, self->props.data);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_tensor_acc_dispose(GObject *object) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    G_OBJECT_CLASS(gva_tensor_acc_parent_class)->dispose(object);
}

static void gva_tensor_acc_finalize(GObject *object) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    gva_tensor_acc_cleanup(self);
    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_tensor_acc_parent_class)->finalize(object);
}

static gboolean gva_tensor_acc_set_caps(GstBaseTransform *base, GstCaps * /*incaps*/, GstCaps * /*outcaps*/) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gva_tensor_acc_sink_event(GstBaseTransform *base, GstEvent *event) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // TODO: push buffer with result on EOS or Flush?

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_acc_parent_class)->sink_event(base, event);
}

static gboolean gva_tensor_acc_start(GstBaseTransform *base) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters:\n -- Mode: %s\n -- Window step: %d\n -- Window size: %d\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)), mode_to_string(self->props.mode).c_str(),
                    self->props.window_step, self->props.window_size);

    self->props.accumulator = create_accumulator(self);
    if (!self->props.accumulator) {
        GST_ERROR_OBJECT(self, "Failed to create accumulator instance");
        return false;
    }

    return true;
}

static gboolean gva_tensor_acc_stop(GstBaseTransform *base) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static gboolean gva_tensor_acc_transform_size(GstBaseTransform *trans, GstPadDirection direction, GstCaps * /*caps*/,
                                              gsize /*size*/, GstCaps * /*othercaps*/, gsize *othersize) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(trans);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* GStreamer hardcoded call with GST_PAD_SINK only */
    g_assert(direction == GST_PAD_SINK);
    *othersize = 0;

    return true;
}

GstCaps *gva_tensor_acc_transform_caps(GstBaseTransform *trans, GstPadDirection direction, GstCaps *caps,
                                       GstCaps *filter) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(trans);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    auto srccaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SRC_PAD(trans));
    auto sinkcaps = gst_pad_get_pad_template_caps(GST_BASE_TRANSFORM_SINK_PAD(trans));

    GstCaps *ret = nullptr;
    switch (direction) {
    case GST_PAD_SINK:
        if (gst_caps_can_intersect(caps, sinkcaps)) {
            try {
                auto sink_tensors_caps_array = TensorCapsArray::FromCaps(caps);
                std::vector<TensorCaps> src_tensor_caps;
                for (size_t i = 0; i < sink_tensors_caps_array.GetTensorNum(); ++i) {
                    auto tensor_desc = sink_tensors_caps_array.GetTensorDesc(i);
                    if (!tensor_desc.HasBatchSize())
                        throw std::logic_error("Unsupported layout format. Can't adjust the dimensions in accordance "
                                               "with the parameters of the window");
                    // TODO: still not fully correct caps
                    // For action recognition we get NCHW 1,512,1,1 and next model expects CHW 1,16,512
                    // Need to think how to construct correct caps
                    auto dims = tensor_desc.GetDims();
                    dims[0] *= self->props.window_size;
                    src_tensor_caps.emplace_back(tensor_desc.GetMemoryType(), tensor_desc.GetPrecision(),
                                                 tensor_desc.GetLayout(), dims);
                }
                ret = TensorCapsArray::ToCaps(TensorCapsArray(src_tensor_caps));
            } catch (const std::exception &e) {
                GST_ERROR_OBJECT(self, "Failed to parse tensor capabilities: %s",
                                 Utils::createNestedErrorMsg(e).c_str());
                return NULL;
            }
        } else {
            ret = gst_caps_new_empty();
        }
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

static GstFlowReturn gva_tensor_acc_transform(GstBaseTransform *base, GstBuffer *inbuf, GstBuffer *outbuf) {
    GvaTensorAcc *self = GVA_TENSOR_ACC(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    /* TODO: is there any way we wil receive more than one memory ? */
    g_assert(gst_buffer_n_memory(inbuf) == 1);
    g_assert(gst_buffer_n_memory(outbuf) == 0);

    self->props.accumulator->accumulate(inbuf);
    auto acc_result = self->props.accumulator->get_result(outbuf);
    switch (self->props.mode) {
    case MODE_SLIDING_WINDOW:
        if (!acc_result) {
            GST_ERROR_OBJECT(self, "Failed to create an accumulation result");
            // TODO: copy mem from inbuf or what should be done if we couldn't accumulate?
            return GST_FLOW_OK;
        }
        return GST_FLOW_OK;
    case MODE_CONDITION:
        if (!acc_result) {
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
        return GST_FLOW_OK;
    default:
        throw std::runtime_error("Unsupported accumulation mode");
    }
}

static void gva_tensor_acc_class_init(GvaTensorAccClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_tensor_acc_set_property;
    gobject_class->get_property = gva_tensor_acc_get_property;
    gobject_class->dispose = gva_tensor_acc_dispose;
    gobject_class->finalize = gva_tensor_acc_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gva_tensor_acc_set_caps;
    base_transform_class->sink_event = gva_tensor_acc_sink_event;
    base_transform_class->start = gva_tensor_acc_start;
    base_transform_class->stop = gva_tensor_acc_stop;
    base_transform_class->transform = gva_tensor_acc_transform;
    base_transform_class->transform_caps = gva_tensor_acc_transform_caps;
    base_transform_class->transform_size = gva_tensor_acc_transform_size;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_ACC_NAME, "application", GVA_TENSOR_ACC_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));

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
    g_object_class_install_property(gobject_class, PROP_ACC_DATA,
                                    g_param_spec_enum("data", "Accumulation data", "Data to accumulate from tensors",
                                                      GST_TYPE_GVA_TENSOR_ACC_DATA, DEFAULT_ACC_DATA,
                                                      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}
