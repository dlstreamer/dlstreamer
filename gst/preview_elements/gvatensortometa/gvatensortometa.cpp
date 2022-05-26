/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
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
#define GST_CAT_DEFAULT gva_tensor_to_meta_debug_category

GType gva_tensor_to_meta_converter_get_type(void) {
    static const GEnumValue converter_types[] = {
        {static_cast<int>(post_processing::ConverterType::TO_ROI), "To ROI", "to_roi"},
        {static_cast<int>(post_processing::ConverterType::TO_TENSOR), "To Tensor", "to_label"},
        {static_cast<int>(post_processing::ConverterType::RAW), "Raw", "raw"},
        {0, nullptr, nullptr}};

    static GType gva_tensor_to_meta_converter_type =
        g_enum_register_static("GvaTensorToMetaConverter", converter_types);
    return gva_tensor_to_meta_converter_type;
}

enum { PROP_0, PROP_MODEL_PROC, PROP_THRESHOLD, PROP_CONVERTER_TYPE, PROP_LABELS };

namespace {
constexpr auto DEFAULT_CONVERTER_TYPE = post_processing::ConverterType::TO_ROI;
constexpr auto DEFAULT_MIN_THRESHOLD = 0.;
constexpr auto DEFAULT_MAX_THRESHOLD = 1.;
constexpr auto DEFAULT_THRESHOLD = 0.5;

std::pair<std::string, std::string> query_model_info(GstPad *pad) {
    auto query = gva_query_new_model_info();
    if (!gst_pad_peer_query(pad, query)) {
        gst_query_unref(query);
        return {};
    }

    std::string model_name;
    std::string instance_id;
    gva_query_parse_model_info(query, model_name, instance_id);
    gst_query_unref(query);

    return std::make_pair(model_name, instance_id);
}

std::vector<TensorLayerDesc> query_model_output(GstPad *pad) {
    std::vector<TensorLayerDesc> result;
    auto query = gva_query_new_model_output();
    if (!gst_pad_peer_query(pad, query)) {
        gst_query_unref(query);
        return result;
    }

    gva_query_parse_model_output(query, result);
    gst_query_unref(query);

    return result;
}

bool query_model_input(GstPad *pad, TensorLayerDesc &desc) {
    auto query = gva_query_new_model_input();
    if (!gst_pad_peer_query(pad, query)) {
        gst_query_unref(query);
        return false;
    }
    auto ret = gva_query_parse_model_input(query, desc);
    gst_query_unref(query);
    return ret;
}

} // namespace

class GvaTensorToMetaPrivate {
  private:
    bool check_stopped() const {
        GstState state;
        bool is_stopped;

        GST_OBJECT_LOCK(_base);
        state = GST_STATE(_base);
        is_stopped = state == GST_STATE_READY || state == GST_STATE_NULL;
        GST_OBJECT_UNLOCK(_base);
        return is_stopped;
    }

    void set_model_proc(const std::string &model_proc_path) {
        if (check_stopped()) {
            _model_proc = model_proc_path;
            GST_INFO_OBJECT(_base, "model-proc: %s", _model_proc.c_str());
        } else {
            GST_ELEMENT_WARNING(
                _base, RESOURCE, SETTINGS, ("'model-proc' can't be changed"),
                ("You cannot change 'model-proc' property on tensor_to_meta when element is running/playing"));
        }
    }

    void set_labels(const std::string &labels) {
        if (check_stopped()) {
            _labels_path = labels;
            GST_INFO_OBJECT(_base, "labels: %s", _labels_path.c_str());
        } else {
            GST_ELEMENT_WARNING(
                _base, RESOURCE, SETTINGS, ("'labels' can't be changed"),
                ("You cannot change 'labels' property on tensor_to_meta when element is running/playing"));
        }
    }

  public:
    static GvaTensorToMetaPrivate *unpack(gpointer object) {
        assert(GVA_TENSOR_TO_META(object)->impl);
        return GVA_TENSOR_TO_META(object)->impl;
    }

    GvaTensorToMetaPrivate(GstBaseTransform *base) : _base(base) {
    }

    void get_property(guint prop_id, GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_MODEL_PROC:
            g_value_set_string(value, _model_proc.c_str());
            break;
        case PROP_THRESHOLD:
            g_value_set_float(value, _threshold);
            break;
        case PROP_CONVERTER_TYPE:
            g_value_set_enum(value, static_cast<int>(_converter_type));
            break;
        case PROP_LABELS:
            g_value_set_string(value, _labels_path.c_str());
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    void set_property(guint prop_id, const GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_MODEL_PROC:
            set_model_proc(g_value_get_string(value));
            break;
        case PROP_THRESHOLD:
            _threshold = g_value_get_float(value);
            break;
        case PROP_CONVERTER_TYPE:
            _converter_type = static_cast<post_processing::ConverterType>(g_value_get_enum(value));
            break;
        case PROP_LABELS:
            set_labels(g_value_get_string(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    bool set_caps(GstCaps *incaps, GstCaps * /*outcaps*/) {
        try {
            _tensor_caps = TensorCapsArray::FromCaps(incaps);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failed to parse tensor capabilities: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }

        try {
            auto model_info = query_model_info(_base->sinkpad);
            _instance_id = model_info.second;
            _model_outputs = query_model_output(_base->sinkpad);

            post_processing::ModelOutputsInfo tensor_descs;
            for (const auto &desc : _model_outputs)
                tensor_descs.emplace(desc.layer_name, desc.dims);

            TensorLayerDesc model_input_desc;
            if (!query_model_input(GST_BASE_TRANSFORM_SINK_PAD(_base), model_input_desc)) {
                GST_ERROR_OBJECT(_base, "Failed to query model input info");
                return false;
            }
            // TODO: assumption that model input is ImageInfo
            TensorCaps model_input_tensor_caps(InferenceBackend::MemoryType::SYSTEM, model_input_desc.precision,
                                               model_input_desc.layout, model_input_desc.dims);
            auto batch_size = model_input_tensor_caps.HasBatchSize() ? model_input_tensor_caps.GetBatchSize() : 1;
            _postproc.reset(new post_processing::PostProcessor(
                model_input_tensor_caps.GetWidth(), model_input_tensor_caps.GetHeight(), batch_size, _model_proc,
                model_info.first /* model name */, tensor_descs, _converter_type, _threshold, _labels_path));
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Error during creating postprocessor: %s", Utils::createNestedErrorMsg(e).c_str());
            return false;
        }

        return true;
    }

    bool query(GstPadDirection direction, GstQuery *query);
    void dispose();

    GstFlowReturn transform_ip(GstBuffer *buffer) {
        ITT_TASK(std::string(GST_ELEMENT_NAME(_base)) + " " + __FUNCTION__);
        GST_DEBUG_OBJECT(_base, "Transform buffer: ts=%" GST_TIME_FORMAT, GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

        try {
            std::vector<post_processing::TensorDesc> outputs;
            outputs.reserve(_model_outputs.size());
            for (const auto &e : _model_outputs) {
                outputs.emplace_back(e.precision, e.layout, e.dims, e.layer_name, e.size);
            }
            _postproc->process(buffer, outputs, _instance_id);
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Error during postprocessing: %s", Utils::createNestedErrorMsg(e).c_str());
            return GST_FLOW_ERROR;
        }

        return GST_FLOW_OK;
    }

  private:
    GstBaseTransform *_base = nullptr;
    /* properties */
    std::string _model_proc;
    std::string _labels_path;
    double _threshold = DEFAULT_THRESHOLD;
    post_processing::ConverterType _converter_type = DEFAULT_CONVERTER_TYPE;

    /* internal */
    TensorCapsArray _tensor_caps;
    std::string _instance_id;
    std::vector<TensorLayerDesc> _model_outputs;
    std::unique_ptr<post_processing::PostProcessor> _postproc;
};

G_DEFINE_TYPE_WITH_PRIVATE(GvaTensorToMeta, gva_tensor_to_meta, GST_TYPE_BASE_TRANSFORM);

void GvaTensorToMetaPrivate::dispose() {
    _postproc.reset();

    G_OBJECT_CLASS(gva_tensor_to_meta_parent_class)->dispose(G_OBJECT(_base));
}

bool GvaTensorToMetaPrivate::query(GstPadDirection direction, GstQuery *query) {
    switch (direction) {
    case GST_PAD_SINK: {
        if (GST_QUERY_TYPE(query) == static_cast<GstQueryType>(GvaQueryTypes::GVA_QUERY_POSTPROC_SRCPAD_INFO)) {
            if (!gva_query_fill_postproc_srcpad(query, _base->srcpad))
                GST_ERROR_OBJECT(_base, "Failed to fill postproc srcpad query");

            return true;
        }
        break;
    }
    default:
        break;
    }

    return GST_BASE_TRANSFORM_CLASS(gva_tensor_to_meta_parent_class)->query(_base, direction, query);
}

static void gva_tensor_to_meta_init(GvaTensorToMeta *self) {
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);
    // Intialization of private data
    auto *priv_memory = gva_tensor_to_meta_get_instance_private(self);
    self->impl = new (priv_memory) GvaTensorToMetaPrivate(&self->base);
}

static void gva_tensor_to_meta_finalize(GObject *object) {
    GvaTensorToMeta *self = GVA_TENSOR_TO_META(object);
    GST_DEBUG_OBJECT(self, "%s", __FUNCTION__);

    if (self->impl) {
        self->impl->~GvaTensorToMetaPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_tensor_to_meta_parent_class)->finalize(object);
}

static void gva_tensor_to_meta_class_init(GvaTensorToMetaClass *klass) {
    auto gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        GvaTensorToMetaPrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        GvaTensorToMetaPrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->dispose = [](GObject *object) { GvaTensorToMetaPrivate::unpack(object)->dispose(); };
    gobject_class->finalize = gva_tensor_to_meta_finalize;

    auto base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = [](GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) -> gboolean {
        return GvaTensorToMetaPrivate::unpack(trans)->set_caps(incaps, outcaps);
    };
    base_transform_class->transform_ip = [](GstBaseTransform *trans, GstBuffer *buf) {
        return GvaTensorToMetaPrivate::unpack(trans)->transform_ip(buf);
    };
    base_transform_class->query = [](GstBaseTransform *trans, GstPadDirection direction, GstQuery *query) -> gboolean {
        return GvaTensorToMetaPrivate::unpack(trans)->query(direction, query);
    };

    auto element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(element_class, GVA_TENSOR_TO_META_NAME, "application",
                                          GVA_TENSOR_TO_META_DESCRIPTION, "Intel Corporation");

    const auto prmflag = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);
    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model proc", "Path to model proc file", "", prmflag));
    g_object_class_install_property(
        gobject_class, PROP_LABELS,
        g_param_spec_string("labels", "Labels",
                            "Path to file containing model's output layer labels or comma separated list of KEY=VALUE "
                            "pairs where KEY is name of output layer and VALUE is path to labels file. If provided, "
                            "labels from model-proc won't be loaded",
                            "", prmflag));

    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float("threshold", "Threshold",
                           "Threshold for detection results. Only regions of interest "
                           "with confidence values above the threshold will be added to the frame",
                           DEFAULT_MIN_THRESHOLD, DEFAULT_MAX_THRESHOLD, DEFAULT_THRESHOLD, prmflag));

    g_object_class_install_property(gobject_class, PROP_CONVERTER_TYPE,
                                    g_param_spec_enum("converter-type", "Converter Type",
                                                      "Postprocessing converter type",
                                                      GST_TYPE_GVA_TENSOR_TO_META_CONVERTER_TYPE,
                                                      static_cast<int>(DEFAULT_CONVERTER_TYPE), prmflag));

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_TENSORS_CAPS)));
}
