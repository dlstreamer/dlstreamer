/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvatensortometa.hpp"

#include <capabilities/tensor_caps.hpp>
#include <frame_data.hpp>
#include <meta/gva_buffer_flags.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

#include <utility>

GST_DEBUG_CATEGORY(gva_tensor_to_meta_debug_category);

G_DEFINE_TYPE(GvaTensorToMeta, gva_tensor_to_meta, GST_TYPE_BASE_TRANSFORM);

GType gva_tensor_to_meta_converter_get_type(void) {
    static GType gva_tensor_to_meta_converter_type = 0;
    static const GEnumValue converter_types[] = {
        {static_cast<int>(post_processing::ConverterType::TO_ROI), "To ROI", "to_roi"},
        {static_cast<int>(post_processing::ConverterType::TO_TENSOR), "To Tensor", "to_label"},
        {static_cast<int>(post_processing::ConverterType::RAW), "Raw", "raw"},
        {0, nullptr, nullptr}};

    if (!gva_tensor_to_meta_converter_type) {
        gva_tensor_to_meta_converter_type = g_enum_register_static("GvaTensorToMetaConverter", converter_types);
    }

    return gva_tensor_to_meta_converter_type;
}

enum { PROP_0, PROP_MODEL_PROC, PROP_THRESHOLD, PROP_CONVERTER_TYPE };

namespace {
constexpr auto DEFAULT_CONVERTER_TYPE = post_processing::ConverterType::TO_ROI;
constexpr auto DEFAULT_MIN_THRESHOLD = 0.;
constexpr auto DEFAULT_MAX_THRESHOLD = 1.;
constexpr auto DEFAULT_THRESHOLD = 0.5;
} // namespace

namespace {
bool check_gva_tensor_to_meta_stopped(GvaTensorToMeta *self) {
    GstState state;
    bool is_stopped;

    GST_OBJECT_LOCK(self);
    state = GST_STATE(self);
    is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
    GST_OBJECT_UNLOCK(self);
    return is_stopped;
}

std::pair<std::string, std::string> query_model_info(GstPad *pad) {
    auto structure = gst_structure_new_empty("model_info");
    auto query = gst_query_new_custom(static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_INFO), structure);
    if (!gst_pad_peer_query(pad, query)) {
        gst_query_unref(query);
        return {};
    }

    auto model_name = gst_structure_get_string(structure, "model_name");
    auto instance_id = gst_structure_get_string(structure, "instance_id");
    return std::make_pair(model_name, instance_id);
}

std::vector<TensorLayerDesc> query_model_output(GstPad *pad) {
    std::vector<TensorLayerDesc> result;
    auto structure = gst_structure_new_empty("model_output");
    auto query = gst_query_new_custom(static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_MODEL_OUTPUT), structure);
    if (!gst_pad_peer_query(pad, query)) {
        gst_query_unref(query);
        return result;
    }

    GArray *output_array;
    gst_structure_get(structure, "outputs", G_TYPE_ARRAY, &output_array, nullptr);
    result.reserve(output_array->len);
    for (auto i = 0u; i < output_array->len; i++)
        result.push_back(g_array_index(output_array, TensorLayerDesc, i));
    gst_query_unref(query);
    return result;
}

} // namespace

static void gva_tensor_to_meta_init(GvaTensorToMeta *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Initialize C++ structure with new
    new (&self->props) GvaTensorToMeta::_Props();

    self->props.threshold = DEFAULT_THRESHOLD;
    self->props.converter_type = DEFAULT_CONVERTER_TYPE;
}

void gva_tensor_to_meta_set_model_proc(GvaTensorToMeta *self, const gchar *model_proc_path) {
    if (check_gva_tensor_to_meta_stopped(self)) {
        self->props.model_proc = g_strdup(model_proc_path);
        GST_INFO_OBJECT(self, "model-proc: %s", self->props.model_proc.c_str());
    } else {
        GST_ELEMENT_WARNING(self, RESOURCE, SETTINGS, ("'model-proc' can't be changed"),
                            ("You cannot change 'model-proc' property on tensor_to_meta when a file is open"));
    }
}

static void gva_tensor_to_meta_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        gva_tensor_to_meta_set_model_proc(self, g_value_get_string(value));
        break;
    case PROP_THRESHOLD:
        self->props.threshold = g_value_get_float(value);
        break;
    case PROP_CONVERTER_TYPE:
        self->props.converter_type = static_cast<post_processing::ConverterType>(g_value_get_enum(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_tensor_to_meta_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    switch (prop_id) {
    case PROP_MODEL_PROC:
        g_value_set_string(value, self->props.model_proc.c_str());
        break;
    case PROP_THRESHOLD:
        g_value_set_float(value, self->props.threshold);
        break;
    case PROP_CONVERTER_TYPE:
        g_value_set_enum(value, static_cast<int>(self->props.converter_type));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gva_tensor_to_meta_dispose(GObject *object) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
    self->props.postproc.reset();

    G_OBJECT_CLASS(gva_tensor_to_meta_parent_class)->dispose(object);
}

static void gva_tensor_to_meta_finalize(GObject *object) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    // Destroy C++ structure manually
    self->props.~_Props();

    G_OBJECT_CLASS(gva_tensor_to_meta_parent_class)->finalize(object);
}

static gboolean gva_tensor_to_meta_set_caps(GstBaseTransform *base, GstCaps *incaps, GstCaps * /*outcaps*/) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    try {
        self->props.tensor_caps = TensorCapsArray::FromCaps(incaps);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    try {
        auto model_info = query_model_info(base->sinkpad);
        self->props.instance_id = model_info.second;
        self->props.model_outputs = query_model_output(base->sinkpad);

        post_processing::ModelOutputsInfo tensor_descs;
        for (const auto &desc : self->props.model_outputs)
            tensor_descs.emplace(desc.layer_name, desc.dims);

        // TODO: support multiple tensor caps
        const auto &first_tensor_caps = self->props.tensor_caps.GetTensorDesc(0);
        self->props.postproc.reset(new post_processing::PostProcessor(
            first_tensor_caps.GetWidth(), first_tensor_caps.GetHeight(), first_tensor_caps.GetBatchSize(),
            self->props.model_proc, model_info.first /* model name */, tensor_descs, self->props.converter_type,
            self->props.threshold));
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during creating postprocessor: %s", Utils::createNestedErrorMsg(e).c_str());
        return false;
    }

    return true;
}

static gboolean gva_tensor_to_meta_sink_event(GstBaseTransform *base, GstEvent *event) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_to_meta_parent_class)->sink_event(base, event);
}

static gboolean gva_tensor_to_meta_start(GstBaseTransform *base) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    GST_INFO_OBJECT(self, "%s parameters:\n -- Model proc: %s\n", GST_ELEMENT_NAME(GST_ELEMENT_CAST(self)),
                    self->props.model_proc.c_str());

    return true;
}

static gboolean gva_tensor_to_meta_stop(GstBaseTransform *base) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    return true;
}

static GstFlowReturn gva_tensor_to_meta_transform_ip(GstBaseTransform *base, GstBuffer *buffer) {
    ITT_TASK(std::string(GST_ELEMENT_NAME(base)) + " " + __FUNCTION__);
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(base);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    try {
        std::vector<post_processing::TensorDesc> outputs;
        outputs.reserve(self->props.model_outputs.size());
        for (const auto &e : self->props.model_outputs) {
            outputs.emplace_back(e.precision, e.layout, e.dims, e.layer_name, e.size);
        }
        self->props.postproc->process(buffer, outputs, self->props.instance_id);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(self, "Error during postprocessing: %s", Utils::createNestedErrorMsg(e).c_str());
        return GST_FLOW_ERROR;
    }

    return GST_FLOW_OK;
}

static void gva_tensor_to_meta_class_init(GvaTensorToMetaClass *klass) {

    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gva_tensor_to_meta_set_property;
    gobject_class->get_property = gva_tensor_to_meta_get_property;
    gobject_class->dispose = gva_tensor_to_meta_dispose;
    gobject_class->finalize = gva_tensor_to_meta_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = gva_tensor_to_meta_set_caps;
    base_transform_class->sink_event = gva_tensor_to_meta_sink_event;
    base_transform_class->start = gva_tensor_to_meta_start;
    base_transform_class->stop = gva_tensor_to_meta_stop;
    base_transform_class->transform_ip = gva_tensor_to_meta_transform_ip;

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_TO_META_NAME, "application",
                                          GVA_TENSOR_TO_META_DESCRIPTION, "Intel Corporation");

    g_object_class_install_property(gobject_class, PROP_MODEL_PROC,
                                    g_param_spec_string("model-proc", "Model proc", "Path to model proc file", nullptr,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float("threshold", "Threshold",
                           "Threshold for detection results. Only regions of interest "
                           "with confidence values above the threshold will be added to the frame",
                           DEFAULT_MIN_THRESHOLD, DEFAULT_MAX_THRESHOLD, DEFAULT_THRESHOLD,
                           (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_CONVERTER_TYPE,
        g_param_spec_enum("converter-type", "Converter Type", "Postprocessing converter type",
                          GST_TYPE_GVA_TENSOR_TO_META_CONVERTER_TYPE, static_cast<int>(DEFAULT_CONVERTER_TYPE),
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
}
