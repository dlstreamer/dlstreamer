/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_inference.h"
#include "dlstreamer/gst/dictionary.h"
#include "elem_names.h"
#include "model_proc_provider.h"

#include <sstream>

// TODO queue sizes?
#define PREPROCESS_QUEUE_SIZE(BATCH_SIZE) (BATCH_SIZE + 2)
#define PROCESS_QUEUE_SIZE(BATCH_SIZE) (BATCH_SIZE + 2)
#define POSTPROCESS_QUEUE_SIZE(BATCH_SIZE) 0 // unlimited
#define AGGREGATE_QUEUE_SIZE(BATCH_SIZE) (BATCH_SIZE + 2)
#define OPENCL_QUEUE_SIZE(BATCH_SIZE) (BATCH_SIZE + 2)

// Debug category
GST_DEBUG_CATEGORY_STATIC(video_inference_debug_category);
#define GST_CAT_DEFAULT video_inference_debug_category

// Register GType
#define video_inference_parent_class parent_class
G_DEFINE_TYPE(VideoInference, video_inference, GST_TYPE_PROCESSBIN);
// G_DEFINE_TYPE_WITH_PRIVATE(VideoInference, video_inference, GST_TYPE_BIN)

#define GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME "gst.vaapi.Display"

/* Properties */
enum {
    PROP_0,
    /* inference */
    PROP_MODEL,
    PROP_IE_CONFIG,
    PROP_DEVICE,
    PROP_INSTANCE_ID,
    PROP_NIREQ,
    PROP_BATCH_SIZE,
    /* pre-post-proc */
    PROP_MODEL_PROC,
    PROP_PRE_PROC_BACKEND,
    PROP_INFERENCE_INTERVAL,
    PROP_ROI_INFERENCE_INTERVAL,
    PROP_REGION,
    PROP_OBJECT_CLASS,
    PROP_LABELS,
    PROP_LABELS_FILE,
    PROP_ATTACH_TENSOR_DATA,
    PROP_THRESHOLD,
    PROP_SCALE_METHOD,
    PROP_REPEAT_METADATA,
};

namespace {

constexpr guint MIN_NIREQ = 0;
constexpr guint MAX_NIREQ = 1024;
constexpr guint DEFAULT_NIREQ = MIN_NIREQ;

constexpr guint MIN_BATCH_SIZE = 0;
constexpr guint MAX_BATCH_SIZE = 1024;
constexpr guint DEFAULT_BATCH_SIZE = 0;

constexpr guint MIN_INFERENCE_INTERVAL = 1;
constexpr guint MAX_INFERENCE_INTERVAL = std::numeric_limits<guint>::max();
constexpr guint DEFAULT_INFERENCE_INTERVAL = 1;

constexpr guint MIN_ROI_INFERENCE_INTERVAL = 1;
constexpr guint MAX_ROI_INFERENCE_INTERVAL = std::numeric_limits<guint>::max();
constexpr guint DEFAULT_ROI_INFERENCE_INTERVAL = 1;

constexpr auto DEFAULT_DEVICE = "CPU";

constexpr auto DEFAULT_INFERENCE_REGION = Region::FULL_FRAME;

constexpr auto DEFAULT_ATTACH_TENSOR_DATA = TRUE;

constexpr auto DEFAULT_REPEAT_METADATA = FALSE;

constexpr auto MAX_THRESHOLD = 1.f;
constexpr auto MIN_THRESHOLD = 0.f;
constexpr auto DEFAULT_THRESHOLD = 0.f;

constexpr auto TORCHVISION_PREFIX = "torchvision.models";
constexpr auto PYTORCH_MODELS_EXT = {".pth", ".pt"};

enum class InferenceBackend { OPEN_VINO, PYTORCH };

#define IS_ESCAPE_SYMBOL(c) (((c) == '\"') || ((c) == '\\'))

inline bool str_ends_with(std::string const &str, std::string const &ending) {
    if (str.length() < ending.length())
        return false;

    return (str.compare(str.length() - ending.length(), ending.length(), ending) == 0);
}

} // namespace

using namespace dlstreamer;

GType preprocess_backend_get_type() {
    static const GEnumValue values[] = {
        {static_cast<int>(PreProcessBackend::AUTO), "Automatic", "auto"},
        {static_cast<int>(PreProcessBackend::GST_OPENCV),
         "GStreamer (primary) and OpenCV (secondary). Pre-processing outputs other/tensors(memory:System)",
         "gst-opencv"},
        {static_cast<int>(PreProcessBackend::VAAPI), "VA-API pre-processing, outputs other/tensors(memory:System)",
         "vaapi"},
        {static_cast<int>(PreProcessBackend::VAAPI_TENSORS),
         "VA-API pre-processing, outputs other/tensors(memory:VASurface)", "vaapi-tensors"},
        {static_cast<int>(PreProcessBackend::VAAPI_SURFACE_SHARING),
         "VA-API pre-processing, outputs video/x-raw(memory:VASurface)", "vaapi-surface-sharing"},
        {static_cast<int>(PreProcessBackend::VAAPI_OPENCL),
         "VA-API (primary) and OpenCL (secondary). Pre-processing outputs other/tensors(memory:OpenCL)",
         "vaapi-opencl"},

        // deprecated values
        {static_cast<int>(PreProcessBackend::GST_OPENCV),
         "DEPRECATED - use auto selection (don't set this property) or set pre-process-backend=gst-opencv", "ie"},
        {static_cast<int>(PreProcessBackend::GST_OPENCV),
         "DEPRECATED - use auto selection (don't set this property) or set pre-process-backend=gst-opencv", "opencv"},
        {0, nullptr, nullptr}};
    return g_enum_register_static("VideoInferenceBackend", values);
}

enum class ScaleMethod { Default, Nearest, Bilinear, Bicubic, Lanczos, Spline, Fast, DlsVaapi };

static GEnumValue scale_method_values[] = {{static_cast<int>(ScaleMethod::Default), "Default", "default"},
                                           {static_cast<int>(ScaleMethod::Nearest), "Nearest", "nearest"},
                                           {static_cast<int>(ScaleMethod::Bilinear), "Bilinear", "bilinear"},
                                           {static_cast<int>(ScaleMethod::Bicubic), "Bicubic", "bicubic"},
                                           {static_cast<int>(ScaleMethod::Lanczos), "Lanczos", "lanczos"},
                                           {static_cast<int>(ScaleMethod::Spline), "Spline", "spline"},
                                           {static_cast<int>(ScaleMethod::Fast), "(VA-API only) fast scale", "fast"},
                                           {static_cast<int>(ScaleMethod::DlsVaapi),
                                            "(VA-API only) scale via Intel® Deep Learning Streamer element",
                                            "dls-vaapi"},
                                           {0, nullptr, nullptr}};

GType scale_method_get_type() {
    return g_enum_register_static("VideoInferenceScaleMethod", scale_method_values);
}

std::string scale_method_to_string(ScaleMethod method) {
    for (GEnumValue *m = scale_method_values; m->value_name; m++) {
        if (m->value == static_cast<int>(method))
            return m->value_name;
    }
    throw std::runtime_error("Unknown ScaleMethod");
}

GType inference_region_get_type() {
    static GEnumValue region_types[] = {
        {static_cast<int>(Region::FULL_FRAME), "Perform inference for full frame", "full-frame"},
        {static_cast<int>(Region::ROI_LIST), "Perform inference for roi list", "roi-list"},
        {0, nullptr, nullptr}};

    static GType video_inference_region_type = g_enum_register_static("VideoInferenceRegion", region_types);
    return video_inference_region_type;
}

static bool structure_has_any_of_fields(GstStructure *structure, const std::vector<std::string_view> &fields) {
    if (!structure)
        return false;
    return std::any_of(fields.begin(), fields.end(),
                       [&](const std::string_view &fld) { return gst_structure_has_field(structure, fld.data()); });
}

template <typename Iter>
static inline std::string join_strings(Iter begin, Iter end, char delimiter = ',') {
    std::ostringstream result;
    for (auto iter = begin; iter != end; iter++) {
        if (iter == begin)
            result << *iter;
        else
            result << delimiter << *iter;
    }
    return result.str();
}

std::string g_value_to_string(const GValue *gval) {
    switch (G_VALUE_TYPE(gval)) {
    case G_TYPE_INT:
        return std::to_string(g_value_get_int(gval));
    case G_TYPE_DOUBLE:
        return std::to_string(g_value_get_double(gval));
    case G_TYPE_BOOLEAN:
        return g_value_get_boolean(gval) ? "true" : "false";
    case G_TYPE_STRING:
        return g_value_get_string(gval);
    default:
        throw std::runtime_error("Unsupported GstStructure field type");
    }
}

static std::string escape_string(const std::string &s) {
    std::ostringstream result;
    result << "\"";

    for (auto &c : s) {
        if (IS_ESCAPE_SYMBOL(c)) {
            result << '\\';
        }
        result << c;
    }

    result << "\"";

    return result.str();
}

static std::string gvalue_serialize(const GValue *gval) {
    gchar *str_ptr = gst_value_serialize(gval);
    if (!str_ptr)
        throw std::runtime_error("Error serializing GValue of type %d" + std::to_string(G_VALUE_TYPE(gval)));

    std::string str(str_ptr);
    g_free(str_ptr);

    if (std::all_of(str.begin(), str.end(), [](char const &c) { return std::isalpha(c); })) {
        return str;
    } else {
        return escape_string(str);
    }
}

static std::string get_structure_value(GstStructure *structure, std::string_view fieldname) {
    const GValue *gval = gst_structure_get_value(structure, fieldname.data());
    if (!gval)
        throw std::runtime_error("Field not found in GstStructure: " + std::string(fieldname));
    return gvalue_serialize(gval);
}

static std::string fields_to_params(GstStructure *structure) {
    std::string str;
    int nfields = structure ? gst_structure_n_fields(structure) : 0;
    for (int i = 0; i < nfields; i++) {
        std::string name = gst_structure_nth_field_name(structure, i);
        if (name == "converter")
            continue;
        std::string value = get_structure_value(structure, name.data());
        std::replace(name.begin(), name.end(), '_', '-'); // property names use '-' as separator
        str += " " + name + "=" + value;
    }
    return str;
}

std::string fields_to_params(GstStructure *structure, const std::vector<std::string_view> &fields) {
    std::string str;
    for (auto &field : fields) {
        if (gst_structure_has_field(structure, field.data())) {
            std::string value = get_structure_value(structure, field.data());
            std::string name(field);
            std::replace(name.begin(), name.end(), '_', '-'); // property names use '-' as separator
            str += " " + name + "=" + value;
        }
    }
    return str;
}

class VideoInferencePrivate {
    friend struct _VideoInference;

  public:
    static std::shared_ptr<VideoInferencePrivate> unpack(gpointer base) {
        g_assert(VIDEO_INFERENCE(base)->impl != nullptr);
        return VIDEO_INFERENCE(base)->impl;
    }

    VideoInferencePrivate(VideoInference *self, GstProcessBin *base) : _self(self), _base(base) {
    }

    ~VideoInferencePrivate() {
    }

    bool link_elements() {
        GObject *gobject = G_OBJECT(_base);
        std::string preprocess, process, postprocess, aggregate, postaggregate;

        if (processbin_is_linked(_base))
            return true;

        if (dlstreamer::get_property_as_string(gobject, "preprocess") == "NULL")
            preprocess = get_preproces_pipeline();

        if (dlstreamer::get_property_as_string(gobject, "process") == "NULL")
            process = get_process_pipeline();

        if (dlstreamer::get_property_as_string(gobject, "postprocess") == "NULL")
            postprocess = get_postprocess_pipeline();

        if (dlstreamer::get_property_as_string(gobject, "aggregate") == "NULL")
            aggregate = elem::meta_aggregate + _aggregate_params;

        if (dlstreamer::get_property_as_string(gobject, "postaggregate") == "NULL")
            postaggregate = _postaggregate_element;

        // TODO set queue size via properties?
        processbin_set_queue_size(_base, PREPROCESS_QUEUE_SIZE(_batch_size), PROCESS_QUEUE_SIZE(_batch_size),
                                  POSTPROCESS_QUEUE_SIZE(_batch_size), AGGREGATE_QUEUE_SIZE(_batch_size), -1);

        // TODO set elements via properties?
        return processbin_set_elements_description(_base, preprocess.data(), process.data(), postprocess.data(),
                                                   aggregate.data(), postaggregate.data());
    }

    PreProcessBackend get_pre_proc_type(GstPad *pad) {
        GstQuery *query = gst_query_new_context(GST_VAAPI_DISPLAY_CONTEXT_TYPE_NAME);
        GstContext *vaapi_context = 0;
        if (gst_pad_peer_query(pad, query))
            gst_query_parse_context(query, &vaapi_context);
        gst_query_unref(query);

        if (vaapi_context) {
#if 0 // TODO enable vaapi-surface-sharing by default
            if (_inference_backend == InferenceBackend::OPEN_VINO && _device.find("GPU") != std::string::npos) {
                return PreProcessBackend::VAAPI_SURFACE_SHARING;
            }
#endif
            return PreProcessBackend::VAAPI;
        } else {
            return PreProcessBackend::GST_OPENCV;
        }
    }

    gboolean sink_event_handler(GstPad *pad, GstEvent *event) {
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS && !processbin_is_linked(_base)) {
            if (_preprocess_backend == PreProcessBackend::AUTO)
                _preprocess_backend = get_pre_proc_type(pad);
            if (!link_elements())
                throw std::runtime_error("Failed to link elements");
        }
        return gst_pad_event_default(pad, GST_OBJECT(_base), event);
    }

    std::string get_preproces_pipeline() {
        std::string pipe;
        std::string separator(elem::pipe_separator);

        // Use capsrelax to avoid propagation of resized image capabilites outside of preprocess pipeline
        pipe += separator + elem::capsrelax;

        // inference_interval
        if (_inference_interval > 1) {
            pipe += separator + elem::rate_adjust + " ratio=1/" + std::to_string(_inference_interval);
        }

        /* Insert roi_split if inference-region=roi-list */
        if (_inference_region == Region::ROI_LIST) {
            pipe += separator + elem::roi_split;
            if (!_object_class.empty())
                pipe += " object-class=" + _object_class;
        }

        // roi_inference_interval
        if (_roi_inference_interval > 1) {
            if (_inference_region != Region::ROI_LIST)
                throw std::runtime_error("Property roi-inference-interval requires inference-region=per-roi");
            pipe += separator + elem::rate_adjust + " ratio=1/" + std::to_string(_roi_inference_interval);
        }

        if (_model_preproc.size() > 1)
            throw std::runtime_error("Only model-proc with single input layer supported"); // TODO
        GstStructure *params = _model_preproc.empty() ? nullptr : _model_preproc.begin()->get()->params;

        // default color format is BGR for OPEN_VINO and RGB for other backends
        const gchar *color_space = (_inference_backend == InferenceBackend::OPEN_VINO) ? "BGR" : "RGB";
        if (params && gst_structure_has_field(params, "color_space"))
            color_space = gst_structure_get_string(params, "color_space");
        auto color_space_to_caps_color_format = [](const std::string &color_space) {
            if (color_space == "BGR")
                return ",format={BGR,BGRP}";
            if (color_space == "RGB")
                return ",format={RGB,RGBP}";
            // TODO: Support grayscale. Other formats?
            throw std::runtime_error("The color space specified in the model-proc file is not supported");
        };
        std::string color_format = color_space_to_caps_color_format(color_space);

        bool keep_aspect_ratio = false;
        if (structure_has_any_of_fields(params, {"resize"}))
            keep_aspect_ratio = gst_structure_get_string(params, "resize") == std::string("aspect-ratio");

        std::string normalization_params;
        if (structure_has_any_of_fields(params, {"range", "mean", "std"}))
            normalization_params = fields_to_params(params, {"range", "mean", "std"});
        // element pytorch_tensor_inference supports only F32 tensors
        if (_inference_backend == InferenceBackend::PYTORCH && normalization_params.empty())
            normalization_params = " range=\"< (double)0, (double)1 >\"";

        /* add preproc elements based on pre-process-backend property */
        switch (_preprocess_backend) {
        case PreProcessBackend::GST_OPENCV:
            // convert parameters naming. TODO other parameters: padding, padding-color, etc
            if (_inference_region == Region::ROI_LIST) {
                // TODO: videoconvert could be removed if opencv_cropscale support more color formats
                pipe += separator + elem::videoconvert;
                pipe += separator + elem::opencv_cropscale;
            } else {
                pipe += separator + elem::videoscale;
            }

            if (keep_aspect_ratio)
                pipe += " add-borders=true";

            if (_scale_method != ScaleMethod::Default)
                pipe += " method=" + scale_method_to_string(_scale_method);
            pipe += separator + elem::videoconvert;
            pipe += separator + elem::caps_system_memory + color_format;
            pipe += separator + elem::tensor_convert;
            if (!normalization_params.empty()) {
                pipe += separator + elem::opencv_tensor_normalize + normalization_params;
            }
            break;
        case PreProcessBackend::VAAPI:
            pipe += separator + elem::caps_vasurface_memory;
            if (_batch_size > 1 || _scale_method == ScaleMethod::DlsVaapi) { // TODO check other parameters
                pipe += separator + elem::batch_create + " batch-size=" + std::to_string(_batch_size);
                pipe += separator + elem::vaapi_batch_proc;
                if (_scale_method != ScaleMethod::Default && _scale_method != ScaleMethod::DlsVaapi)
                    pipe += " scale-method=" + scale_method_to_string(_scale_method) + " ";
            } else {
                pipe += separator + elem::vaapipostproc;
                if (_scale_method != ScaleMethod::Default)
                    pipe += " scale-method=" + scale_method_to_string(_scale_method) + " ";
                pipe += separator + elem::videoconvert;
                pipe += separator + elem::caps_system_memory + color_format;
                pipe += separator + elem::tensor_convert;
            }
            if (!normalization_params.empty()) {
                pipe += separator + elem::opencv_tensor_normalize + normalization_params;
            }
            break;
        case PreProcessBackend::VAAPI_SURFACE_SHARING:
            pipe += separator + elem::caps_vasurface_memory;
            pipe += separator + elem::vaapipostproc;
            if (_scale_method != ScaleMethod::Default)
                pipe += " scale-method=" + scale_method_to_string(_scale_method) + " ";
            if (_batch_size > 1)
                pipe += separator + elem::batch_create + " batch-size=" + std::to_string(_batch_size);
            break;
        case PreProcessBackend::VAAPI_TENSORS:
            // TODO batch-size
            pipe += separator + elem::caps_vasurface_memory;
            if (_scale_method == ScaleMethod::DlsVaapi) {
                pipe += separator + elem::vaapi_batch_proc;
            } else {
                pipe += separator + elem::vaapipostproc;
                if (_scale_method != ScaleMethod::Default)
                    pipe += " scale-method=" + scale_method_to_string(_scale_method) + " ";
                pipe += separator + elem::tensor_convert;
            }
            break;
        case PreProcessBackend::VAAPI_OPENCL:
            pipe += separator + elem::caps_vasurface_memory;
            if (_batch_size > 1) {
                pipe += separator + elem::batch_create + " batch-size=" + std::to_string(_batch_size);
                pipe += separator + elem::vaapi_batch_proc;
            } else {
                if (_scale_method == ScaleMethod::DlsVaapi)
                    pipe += separator + elem::vaapi_batch_proc;
                else
                    pipe += separator + elem::vaapipostproc;
            }
            if (_scale_method != ScaleMethod::Default && _scale_method != ScaleMethod::DlsVaapi)
                pipe += " scale-method=" + scale_method_to_string(_scale_method) + " ";
            pipe += separator + elem::vaapi_to_opencl;
            pipe += separator + elem::queue + " max-size-bytes=0 max-size-time=0 max-size-buffers=" +
                    std::to_string(OPENCL_QUEUE_SIZE(_batch_size));
            pipe += separator + elem::opencl_tensor_normalize;
            break;
        default:
            throw std::runtime_error("Unexpected preproc_backend type");
        }

        pipe.erase(0, separator.size()); // remove " ! " in the beginning
        return pipe;
    }

    std::string get_process_pipeline() {
        return _inference_element + _inference_params;
    }

    // Constructs post-processing sub-pipeline based either on model-proc file or properties or defaults
    std::string get_postprocess_pipeline() {
        std::ostringstream pipe;

        if (_batch_size > 1)
            pipe << elem::batch_split << elem::pipe_separator;

        if (!_model_postproc.empty()) { // Go through all post-processors in model-proc file
            for (auto postproc = _model_postproc.begin(); postproc != _model_postproc.end(); postproc++) {
                if (postproc != _model_postproc.begin())
                    pipe << elem::pipe_separator;

                GstStructure *structure = postproc->second;
                assert(structure);
                // std::string_view cannot be initialized with nullptr
                std::string_view converter = [s = gst_structure_get_string(structure, "converter")] {
                    return s ? s : "";
                }();
                if (converter.empty()) {
                    if (structure_has_any_of_fields(structure, {"labels", "labels_file"}))
                        converter = "label";
                    else
                        converter = "add_params";
                }

                // Element name
                pipe << elem::tensor_postproc_ << converter;

                // properties serialized from GstStruct
                pipe << fields_to_params(structure);
            }
        } else { // default postprocess element
            auto self = VIDEO_INFERENCE_GET_CLASS(_self);
            if (self->get_default_postprocess_elements) {
                pipe << self->get_default_postprocess_elements(_self);
            } else {
                if (_labels.empty() && _labels_file.empty()) {
                    pipe << "tensor_postproc_add_params";
                } else {
                    pipe << "tensor_postproc_label";
                }
            }
        }

        // properties
        if (_threshold != DEFAULT_THRESHOLD) {
            if (_model_postproc.size() > 1)
                throw std::runtime_error("Property 'threshold' is incompatible with multi-layer model proc file");
            pipe << " threshold=" << _threshold;
        }
        if (!_labels.empty()) {
            if (_model_postproc.size() > 1)
                throw std::runtime_error("Property 'labels' is incompatible with multi-layer model proc file");
            pipe << " labels=" << _labels;
        }
        if (!_labels_file.empty()) {
            if (_model_postproc.size() > 1)
                throw std::runtime_error("Property 'labels-file' is incompatible with multi-layer model proc file");
            pipe << " labels-file=" << _labels_file;
        }

        // repeat-metadata
        if (_repeat_metadata)
            pipe << elem::pipe_separator << elem::meta_smooth;

        return naming_replacements(pipe.str());
    }

    std::string naming_replacements(std::string str) {
        static std::vector<std::pair<std::string, std::string>> replacements = {
            {"tensor_postproc_detection_output", "tensor_postproc_detection"},
            {"tensor_postproc_boxes_labels", "tensor_postproc_detection"},
            {"tensor_postproc_boxes", "tensor_postproc_detection"},
            {"tensor_postproc_yolo_v3", "tensor_postproc_yolo version=3"},
            {"tensor_postproc_yolo_v4", "tensor_postproc_yolo version=4"},
            {"tensor_postproc_yolo_v5", "tensor_postproc_yolo version=5"},
            {"tensor_postproc_keypoints_openpose", "tensor_postproc_human_pose"}};
        for (auto &repl : replacements) {
            auto index = str.find(repl.first);
            if (index != std::string::npos)
                str.replace(index, repl.first.length(), repl.second);
        }
        return str;
    }

    void get_property(guint prop_id, GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_MODEL:
            g_value_set_string(value, _model.c_str());
            break;
        case PROP_IE_CONFIG:
            g_value_set_string(value, _ie_config.c_str());
            break;
        case PROP_DEVICE:
            g_value_set_string(value, _device.c_str());
            break;
        case PROP_INSTANCE_ID:
            g_value_set_string(value, _instance_id.c_str());
            break;
        case PROP_NIREQ:
            g_value_set_uint(value, _nireq);
            break;
        case PROP_BATCH_SIZE:
            g_value_set_uint(value, _batch_size);
            break;
        case PROP_MODEL_PROC:
            g_value_set_string(value, _model_proc.c_str());
            break;
        case PROP_PRE_PROC_BACKEND:
            g_value_set_enum(value, static_cast<gint>(_preprocess_backend));
            break;
        case PROP_INFERENCE_INTERVAL:
            g_value_set_uint(value, _inference_interval);
            break;
        case PROP_ROI_INFERENCE_INTERVAL:
            g_value_set_uint(value, _roi_inference_interval);
            break;
        case PROP_REGION:
            g_value_set_enum(value, static_cast<gint>(_inference_region));
            break;
        case PROP_OBJECT_CLASS:
            g_value_set_string(value, _object_class.c_str());
            break;
        case PROP_LABELS:
            gst_value_deserialize(value, _labels.data());
            break;
        case PROP_LABELS_FILE:
            g_value_set_string(value, _labels_file.c_str());
            break;
        case PROP_ATTACH_TENSOR_DATA:
            g_value_set_boolean(value, _attach_tensor_data);
            break;
        case PROP_THRESHOLD:
            g_value_set_float(value, _threshold);
            break;
        case PROP_SCALE_METHOD:
            g_value_set_enum(value, static_cast<gint>(_scale_method));
            break;
        case PROP_REPEAT_METADATA:
            g_value_set_boolean(value, _repeat_metadata);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    void set_property(guint prop_id, const GValue *value, GParamSpec *pspec) {
        switch (prop_id) {
        case PROP_MODEL:
            _model = g_value_get_string(value);
            _inference_params += " model=" + _model;
            if ((_model.rfind(TORCHVISION_PREFIX, 0) == 0) ||
                std::any_of(PYTORCH_MODELS_EXT.begin(), PYTORCH_MODELS_EXT.end(),
                            [&](const std::string &ext) { return str_ends_with(_model, ext); })) {
                _inference_backend = InferenceBackend::PYTORCH;
                _inference_element = elem::pytorch_tensor_inference;
            }
            break;
        case PROP_IE_CONFIG:
            _ie_config = g_value_get_string(value);
            _inference_params += " config=" + _ie_config;
            break;
        case PROP_DEVICE:
            _device = g_value_get_string(value);
            _inference_params += " device=" + _device;
            break;
        case PROP_INSTANCE_ID:
            _instance_id = g_value_get_string(value);
            _inference_params += " shared-instance-id=" + _instance_id;
            break;
        case PROP_NIREQ:
            _nireq = g_value_get_uint(value);
            _inference_params += " buffer-pool-size=" + std::to_string(_nireq);
            break;
        case PROP_BATCH_SIZE:
            _batch_size = g_value_get_uint(value);
            _inference_params += " batch-size=" + std::to_string(_batch_size);
            break;
        case PROP_MODEL_PROC:
            _model_proc = g_value_get_string(value);
            _model_proc_provider.readJsonFile(_model_proc);
            _model_preproc = _model_proc_provider.parseInputPreproc();
            _model_postproc = _model_proc_provider.parseOutputPostproc();
            // change GstStructure name according to "attribute_name" field
            for (auto &preproc : _model_preproc) {
                auto name = gst_structure_get_string(preproc->params, "attribute_name");
                if (name)
                    gst_structure_set_name(preproc->params, name);
            }
            break;
        case PROP_PRE_PROC_BACKEND:
            _preprocess_backend = static_cast<PreProcessBackend>(g_value_get_enum(value));
            break;
        case PROP_INFERENCE_INTERVAL:
            _inference_interval = g_value_get_uint(value);
            break;
        case PROP_ROI_INFERENCE_INTERVAL:
            _roi_inference_interval = g_value_get_uint(value);
            break;
        case PROP_REGION:
            _inference_region = static_cast<Region>(g_value_get_enum(value));
            break;
        case PROP_OBJECT_CLASS:
            _object_class = g_value_get_string(value);
            break;
        case PROP_LABELS:
            _labels = gvalue_serialize(value);
            break;
        case PROP_LABELS_FILE:
            _labels_file = g_value_get_string(value);
            break;
        case PROP_ATTACH_TENSOR_DATA:
            _attach_tensor_data = g_value_get_boolean(value);
            _aggregate_params = _aggregate_params + " attach-tensor-data=" + (_attach_tensor_data ? "true" : "false");
            break;
        case PROP_THRESHOLD:
            _threshold = g_value_get_float(value);
            break;
        case PROP_SCALE_METHOD:
            _scale_method = static_cast<ScaleMethod>(g_value_get_enum(value));
            break;
        case PROP_REPEAT_METADATA:
            _repeat_metadata = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    GstStateChangeReturn change_state(GstStateChange transition) {
        switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY: {
            if (_preprocess_backend != PreProcessBackend::AUTO) {
                g_return_val_if_fail(link_elements(), GST_STATE_CHANGE_FAILURE);
            }
        } break;
        default:
            break;
        }

        return GST_ELEMENT_CLASS(parent_class)->change_state(GST_ELEMENT(_base), transition);
    }

  private:
    VideoInference *_self = nullptr;
    GstProcessBin *_base = nullptr;

    /* properties */
    std::string _model;
    std::string _ie_config;
    std::string _device = DEFAULT_DEVICE;
    std::string _instance_id;
    guint _nireq = DEFAULT_NIREQ;
    guint _batch_size = DEFAULT_BATCH_SIZE;
    guint _inference_interval = DEFAULT_INFERENCE_INTERVAL;
    guint _roi_inference_interval = DEFAULT_ROI_INFERENCE_INTERVAL;
    gboolean _attach_tensor_data = DEFAULT_ATTACH_TENSOR_DATA;
    PreProcessBackend _preprocess_backend = PreProcessBackend::AUTO;
    Region _inference_region = DEFAULT_INFERENCE_REGION;
    ScaleMethod _scale_method = ScaleMethod::Default;
    std::string _object_class;
    std::string _labels;
    std::string _labels_file;
    gboolean _repeat_metadata = DEFAULT_REPEAT_METADATA;

    /* pipeline params */
    InferenceBackend _inference_backend = InferenceBackend::OPEN_VINO;
    std::string _inference_element = elem::openvino_tensor_inference;
    std::string _postaggregate_element;
    std::string _inference_params;
    std::string _aggregate_params;
    float _threshold = DEFAULT_THRESHOLD;

    /* model proc */
    std::string _model_proc;
    ModelProcProvider _model_proc_provider;
    std::vector<ModelInputProcessorInfo::Ptr> _model_preproc;
    std::map<std::string, GstStructure *> _model_postproc;
};

const gchar *VideoInference::get_model() {
    return impl->_model.data();
}

void VideoInference::set_inference_element(const gchar *element) {
    impl->_inference_element = element;
}

void VideoInference::set_postaggregate_element(const gchar *element) {
    impl->_postaggregate_element = element;
}

static void video_inference_init(VideoInference *self) {
    self->impl = std::make_shared<VideoInferencePrivate>(self, &self->base);

    gst_pad_set_event_function(self->base.sink_pad, [](GstPad *pad, GstObject *parent, GstEvent *event) {
        return VIDEO_INFERENCE(parent)->impl->sink_event_handler(pad, event);
    });
}

static void video_inference_finalize(GObject *object) {
    VideoInference *self = VIDEO_INFERENCE(object);
    self->impl.reset();

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void video_inference_class_init(VideoInferenceClass *klass) {
    GST_DEBUG_CATEGORY_INIT(video_inference_debug_category, "gvainference", 0, "Debug category of gvainference");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    klass->get_default_postprocess_elements = nullptr;

    element_class->change_state = [](GstElement *element, GstStateChange transition) {
        return VideoInferencePrivate::unpack(element)->change_state(transition);
    };

    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        return VideoInferencePrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        return VideoInferencePrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->finalize = video_inference_finalize;

    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string("video/x-raw(ANY)")));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string("video/x-raw(ANY)")));

    GParamFlags prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    /* inference properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL,
        g_param_spec_string("model", "Model", "Path to inference model network file", "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for inference configuration", "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string("device", "Device",
                            "Target device for inference. Please see inference backend documentation (ex, OpenVINO™ "
                            "Toolkit) for list of supported devices.",
                            DEFAULT_DEVICE, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_INSTANCE_ID,
        g_param_spec_string(
            "model-instance-id", "Instance-Id",
            "Identifier for sharing resources between inference elements of the same type. Elements with "
            "the instance-id will share model and other properties. If not specified, a unique identifier will be "
            "generated.",
            "", prm_flags));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "Number-Inference-Requests",
                                                      "Maximum number of inference requests running in parallel",
                                                      MIN_NIREQ, MAX_NIREQ, DEFAULT_NIREQ, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_BATCH_SIZE,
        g_param_spec_uint("batch-size", "Batch Size",
                          "Number of frames batched together for a single inference."
                          " If the batch-size is 0, then it will be set by default to be optimal for the device."
                          " Not all models support batching."
                          " Use model optimizer to ensure that the model has batching support.",
                          MIN_BATCH_SIZE, MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE, prm_flags));

    /* pre-post-proc properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string(
            "model-proc", "Model-Proc-File",
            "Path to JSON file with parameters describing how to build pre-process and post-process sub-pipelines", "",
            prm_flags));

    g_object_class_install_property(gobject_class, PROP_PRE_PROC_BACKEND,
                                    g_param_spec_enum("pre-process-backend", "Pre-Process-Backend",
                                                      "Preprocessing backend type", preprocess_backend_get_type(),
                                                      static_cast<int>(PreProcessBackend::AUTO), prm_flags));

    g_object_class_install_property(gobject_class, PROP_INFERENCE_INTERVAL,
                                    g_param_spec_uint("inference-interval", "Inference interval",
                                                      "Run inference for every Nth frame", MIN_INFERENCE_INTERVAL,
                                                      MAX_INFERENCE_INTERVAL, DEFAULT_INFERENCE_INTERVAL, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_ROI_INFERENCE_INTERVAL,
        g_param_spec_uint("roi-inference-interval", "Roi Inference Interval",
                          "Determines how often to run inference on each ROI object. Only valid if each ROI object has "
                          "unique object id (requires object tracking after object detection)",
                          MIN_ROI_INFERENCE_INTERVAL, MAX_ROI_INFERENCE_INTERVAL, DEFAULT_ROI_INFERENCE_INTERVAL,
                          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(
        gobject_class, PROP_REGION,
        g_param_spec_enum("inference-region", "Inference-Region",
                          "Region on which inference will be performed - full-frame or on each ROI (region of interest)"
                          "bounding-box area",
                          inference_region_get_type(), static_cast<gint>(DEFAULT_INFERENCE_REGION), prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "Object-Class",
                            "Run inference only on Region-Of-Interest with specified object class", "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_LABELS,
        g_param_spec_string("labels", "Labels",
                            "Path to file containing model's output layer labels or comma separated list of KEY=VALUE "
                            "pairs where KEY is name of output layer and VALUE is path to labels file. If provided, "
                            "labels from model-proc won't be loaded",
                            "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_LABELS_FILE,
        g_param_spec_string("labels-file", "Labels file",
                            "Path to file containing model's output layer labels. If provided, labels from model-proc "
                            "won't be loaded",
                            "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_ATTACH_TENSOR_DATA,
        g_param_spec_boolean("attach-tensor-data", "Attach-Tensor-Sata",
                             "If true, metadata will contain both post-processing results and raw tensor data. "
                             "If false, metadata will contain post-processing results only.",
                             DEFAULT_ATTACH_TENSOR_DATA, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float("threshold", "Threshold",
                           "Threshold for detection results. Only regions of interest with confidence values above the "
                           "threshold will be added to the frame. Zero means default (auto-selected) threshold",
                           MIN_THRESHOLD, MAX_THRESHOLD, DEFAULT_THRESHOLD,
                           static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    g_object_class_install_property(
        gobject_class, PROP_SCALE_METHOD,
        g_param_spec_enum("scale-method", "scale-method", "Scale method to use in pre-preprocessing before inference",
                          scale_method_get_type(), static_cast<int>(ScaleMethod::Default), prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_REPEAT_METADATA,
        g_param_spec_boolean("repeat-metadata", "repeat-metadata",
                             "If true and inference-interval > 1, metadata with last inference results will be "
                             "attached to frames if inference skipped. "
                             "If true and roi-inference-interval > 1, it requires object-id for each roi, "
                             "so requires object tracking element inserted before this element.",
                             DEFAULT_REPEAT_METADATA, prm_flags));

    gst_element_class_set_metadata(element_class, VIDEO_INFERENCE_NAME, "video", VIDEO_INFERENCE_DESCRIPTION,
                                   "Intel Corporation");
}
