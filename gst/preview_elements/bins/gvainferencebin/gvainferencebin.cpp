/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gvainferencebin.hpp"

#include "dlstreamer/gst/transform.h"
#include "model_proc_provider.h"
#include <feature_toggling/ifeature_toggle.h>
#include <gva_caps.h>
#include <post_processor/post_proc_common.h>
#include <runtime_feature_toggler.h>

#include <list>

GST_DEBUG_CATEGORY(gva_inference_bin_debug_category);
#define GST_CAT_DEFAULT gva_inference_bin_debug_category

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
    PROP_INTERVAL,
    PROP_REGION,
    PROP_OBJECT_CLASS,
    PROP_RESHAPE,
    PROP_RESHAPE_WIDTH,
    PROP_RESHAPE_HEIGHT,
    PROP_NO_BLOCK,
    PROP_PRE_PROC_CONFIG,
    PROP_DEVICE_EXTENSIONS,
    PROP_LABELS,
    PROP_CPU_THROUGHPUT_STREAMS,
    PROP_GPU_THROUGHPUT_STREAMS
};

namespace elem {
constexpr const char *video_roi_split = "video_roi_split";
constexpr const char *rate_adjust = "rate_adjust";
constexpr const char *video_preproc_vaapi = "video_preproc_vaapi"; // TODO not used currently
constexpr const char *video_preproc_vaapi_opencl = "video_preproc_vaapi_opencl";
constexpr const char *tensor_convert = "tensor_convert";
constexpr const char *tensor_normalize_opencv = "tensor_normalize_opencv";
constexpr const char *tensor_normalize_opencl = "tensor_normalize_opencl";
constexpr const char *tensor_inference_openvino = "tensor_inference_openvino";
constexpr const char *tensor_postproc_ = "tensor_postproc_";
constexpr const char *meta_aggregate = "meta_aggregate";

constexpr const char *queue = "queue";
constexpr const char *videoscale = "videoscale";
constexpr const char *videoconvert = "videoconvert";
constexpr const char *vaapipostproc = "vaapipostproc";
constexpr const char *capsfilter = "capsfilter";
} // namespace elem

namespace {

constexpr guint MIN_NIREQ = 0;
constexpr guint MAX_NIREQ = 1024;
constexpr guint DEFAULT_NIREQ = MIN_NIREQ;

// TODO: batch should 0 by default (automatic)
constexpr guint MIN_BATCH_SIZE = 0;
constexpr guint MAX_BATCH_SIZE = 1024;
// constexpr guint DEFAULT_BATCH_SIZE = MIN_BATCH_SIZE;

constexpr guint MIN_INTERVAL = 1;
constexpr guint MAX_INTERVAL = std::numeric_limits<guint>::max();
constexpr guint DEFAULT_INTERVAL = 1;

constexpr auto DEFAULT_DEVICE = "CPU";

constexpr auto DEFAULT_RESHAPE = false;

constexpr auto MIN_RESHAPE_WIDTH = 0;
constexpr auto MAX_RESHAPE_WIDTH = UINT_MAX;
constexpr auto DEFAULT_RESHAPE_WIDTH = 0;

constexpr auto MIN_RESHAPE_HEIGHT = 0;
constexpr auto MAX_RESHAPE_HEIGHT = UINT_MAX;
constexpr auto DEFAULT_RESHAPE_HEIGHT = 0;

constexpr auto DEFAULT_NO_BLOCK = false;
constexpr auto DEFAULT_INFERENCE_REGION = Region::FULL_FRAME;

constexpr auto DEFAULT_THROUGHPUT_STREAMS = 0;
constexpr auto MIN_THROUGHPUT_STREAMS = 0;
constexpr auto MAX_THROUGHPUT_STREAMS = UINT_MAX;

constexpr auto PRE_PROC_AUTO_NAME = "auto";
constexpr auto PRE_PROC_IE_NAME = "ie";
constexpr auto PRE_PROC_GST_NAME = "gst";
constexpr auto PRE_PROC_VAAPI_NAME = "vaapi";
constexpr auto PRE_PROC_VAAPI_OPENCL_NAME = "vaapi-opencl";
constexpr auto PRE_PROC_VAAPI_SURFACE_SHARING_NAME = "vaapi-surface-sharing";
constexpr auto PRE_PROC_OPENCV_LEGACY_NAME = "opencv";

CREATE_FEATURE_TOGGLE(UseMicroElements, "use-micro-elements",
                      "By default gvainference, gvadetect and gvaclassify use legacy elements. If you want to try new "
                      "micro elements approach set environment variable ENABLE_GVA_FEATURES=use-micro-elements.");

CREATE_FEATURE_TOGGLE(UseCPPElements, "use-cpp-elements", "Use elements implemented via C++ internal API");

bool check_use_legacy(bool *use_cpp) {
    using namespace FeatureToggling::Runtime;
    RuntimeFeatureToggler toggler;
    toggler.configure(EnvironmentVariableOptionsReader().read("ENABLE_GVA_FEATURES"));
    *use_cpp = toggler.enabled(UseCPPElements::id);
    bool use_micro = toggler.enabled(UseMicroElements::id);
    return !use_micro && !*use_cpp;
}

bool USE_CPP_ELEMENTS = false;
const bool USE_LEGACY_ELEMENT = check_use_legacy(&USE_CPP_ELEMENTS);
// TODO: batch should 0 by default (automatic)
const uint32_t DEFAULT_BATCH_SIZE = USE_LEGACY_ELEMENT ? 0 : 1;

const gchar *get_caps_str_from_memory_type(CapsFeature mem_type) {
    if (mem_type == VA_SURFACE_CAPS_FEATURE)
        return "video/x-raw(" VASURFACE_FEATURE_STR ")";
    if (mem_type == DMA_BUF_CAPS_FEATURE)
        return "video/x-raw(" DMABUF_FEATURE_STR ")";
    return "video/x-raw";
}

} // namespace

GType gva_inference_bin_backend_get_type(void) {
    static GEnumValue backend_types[] = {
        {static_cast<int>(PreProcessBackend::AUTO), "Automatic", PRE_PROC_AUTO_NAME},
        {static_cast<int>(PreProcessBackend::IE), "Inference Engine", PRE_PROC_IE_NAME},
        {static_cast<int>(PreProcessBackend::GST), "GStreamer", PRE_PROC_GST_NAME},
#ifdef ENABLE_VAAPI
        {static_cast<int>(PreProcessBackend::VAAPI), "VAAPI", PRE_PROC_VAAPI_NAME},
        {static_cast<int>(PreProcessBackend::VAAPI_SURFACE_SHARING), "VAAPI Surface Sharing",
         PRE_PROC_VAAPI_SURFACE_SHARING_NAME},
        {static_cast<int>(PreProcessBackend::VAAPI_OPENCL), "VAAPI (with OpenCL memory output)",
         PRE_PROC_VAAPI_OPENCL_NAME},
#endif
        {0, nullptr, nullptr}};

    if (USE_LEGACY_ELEMENT) {
        backend_types[2] = {static_cast<int>(PreProcessBackend::OPENCV_LEGACY), "OpenCV", PRE_PROC_OPENCV_LEGACY_NAME};
#ifdef ENABLE_VAAPI
        // "Erase" vaapi-opencl
        backend_types[5] = {0, nullptr, nullptr};
#endif
    }

    static GType gva_inference_bin_backend_type = g_enum_register_static("GvaInferenceBinBackend", backend_types);
    return gva_inference_bin_backend_type;
}

GType gva_inference_bin_region_get_type() {
    static GEnumValue region_types[] = {
        {static_cast<int>(Region::FULL_FRAME), "Perform inference for full frame", "full-frame"},
        {static_cast<int>(Region::ROI_LIST), "Perform inference for roi list", "roi-list"},
        {0, nullptr, nullptr}};

    static GType gva_inference_bin_region_type = g_enum_register_static("GvaInferenceBinRegion", region_types);
    return gva_inference_bin_region_type;
}

GstElement *create_element(const std::string &name, const std::map<std::string, std::string> &props) {
    auto element = gst_element_factory_make(name.c_str(), nullptr);
    if (!element)
        throw std::runtime_error("Error creating element: " + name);
    for (const auto &prop : props)
        gst_util_set_object_arg(G_OBJECT(element), prop.first.c_str(), prop.second.c_str());
    return element;
}

class GvaInferenceBinPrivate {
  private:
    bool link_elements(PreProcessBackend linkage);

    bool set_legacy_element_properties() {
        if (_linked)
            return true;

        std::string pre_proc_string;
        if (_pre_proc_backend != PreProcessBackend::AUTO)
            pre_proc_string = g_enum_get_value(G_ENUM_CLASS(g_type_class_peek(gva_inference_bin_backend_get_type())),
                                               static_cast<gint>(_pre_proc_backend))
                                  ->value_nick;

        g_object_set(_legacy_inference, "model", _model.c_str(), "model-proc",
                     _model_proc.empty() ? nullptr : _model_proc.c_str(), "batch_size", _batch_size, "device",
                     _device.c_str(), "device-extensions", _device_extensions.c_str(), "ie-config", _ie_config.c_str(),
                     "inference-interval", _interval, "labels", _labels_path.empty() ? nullptr : _labels_path.c_str(),
                     "model-instance-id", _instance_id.empty() ? nullptr : _instance_id.c_str(), "nireq", _nireq,
                     "no-block", _no_block, "object-class", _object_class.empty() ? nullptr : _object_class.c_str(),
                     "pre-process-config", _preprocess_config.c_str(), "reshape", _reshape, "reshape-height",
                     _reshape_height, "reshape-width", _reshape_width, "pre-process-backend", pre_proc_string.c_str(),
                     "inference-region", _inference_region, "cpu-throughput-streams", _cpu_throughput_streams,
                     "gpu-throughput-streams", _gpu_throughput_streams, nullptr);

        _linked = true;
        return true;
    }

    gboolean sink_event_handler(GstPad *pad, GstEvent *event) {
        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS && _pre_proc_backend == PreProcessBackend::AUTO) {
            GstCaps *caps = nullptr;
            gst_event_parse_caps(event, &caps);
            link_elements(get_pre_proc_type(caps));
        }
        return gst_pad_event_default(pad, GST_OBJECT(_base), event);
    }

    gboolean sink_query_handler(GstPad *pad, GstQuery *query) {
        auto res = gst_pad_query_default(pad, GST_OBJECT(_base), query);
        if (GST_QUERY_TYPE(query) == GST_QUERY_CAPS) {
            GstCaps *resultCaps;
            gst_query_parse_caps_result(query, &resultCaps);
            for (guint i = 0; i < gst_caps_get_size(resultCaps); ++i) {
                GstStructure *structure = gst_caps_get_structure(resultCaps, i);
                gst_structure_remove_field(structure, "width");
                gst_structure_remove_field(structure, "height");
            }
            gst_query_set_caps_result(query, resultCaps);
        }
        return res;
    }

    bool add_element_to_bin(GstElement *element) {
        if (gst_element_get_parent(element))
            return true;
        if (!gst_bin_add(_base, element)) {
            GST_ERROR_OBJECT(_base, "Failed to add element to bin: %s", GST_ELEMENT_NAME(element));
            gst_object_unref(element);
            return false;
        }
        if (!gst_element_sync_state_with_parent(element)) {
            GST_ERROR_OBJECT(_base, "Failed to sync element state with parent: %s", GST_ELEMENT_NAME(element));
            gst_object_unref(element);
            return false;
        }
        return true;
    }

    GstElement *create_caps(CapsFeature capsFeature) {
        return create_element(elem::capsfilter, {{"caps", get_caps_str_from_memory_type(capsFeature)}});
    }

  public:
    static GvaInferenceBinPrivate *unpack(gpointer base) {
        g_assert(GVA_INFERENCE_BIN(base)->impl);
        return GVA_INFERENCE_BIN(base)->impl;
    }

    GvaInferenceBinPrivate(GstBin *base) : _base(base) {
    }

    GstElement *create_legacy_element() {
        return gst_element_factory_make("gvainference_legacy", nullptr);
    }

    bool init_preprocessing(PreProcessBackend linkage, std::list<GstElement *> &link_order) {
        /* enable inference on ROI optionally */
        if (_inference_region == Region::ROI_LIST) {
            std::map<std::string, std::string> props;
            if (!_object_class.empty())
                props["object-class"] = _object_class;
            link_order.push_back(create_element(elem::video_roi_split, props));
        }

        /* add preproc elements based on pre process backend option */
        switch (linkage) {
        case PreProcessBackend::VAAPI_SURFACE_SHARING:
            if (_inference_region == Region::ROI_LIST) {
                link_order.push_back(create_element("preproc_vaapi", {{"crop-roi", "true"}}));
                link_order.push_back(create_caps(VA_SURFACE_CAPS_FEATURE));
            } else {
                link_order.push_back(create_caps(VA_SURFACE_CAPS_FEATURE));
                link_order.push_back(create_element(elem::vaapipostproc));
            }
            break;
        case PreProcessBackend::VAAPI:
            if (_inference_region == Region::ROI_LIST) {
                link_order.push_back(create_element("preproc_vaapi", {{"crop-roi", "true"}}));
                link_order.push_back(create_caps(SYSTEM_MEMORY_CAPS_FEATURE));
            } else {
                link_order.push_back(create_caps(VA_SURFACE_CAPS_FEATURE));
                link_order.push_back(create_element(elem::vaapipostproc));
                link_order.push_back(create_caps(SYSTEM_MEMORY_CAPS_FEATURE));
                link_order.push_back(create_element(elem::videoconvert));
            }
            break;
        case PreProcessBackend::GST:
            if (_inference_region == Region::ROI_LIST) {
                link_order.push_back(create_element("preproc_opencv", {{"crop-roi", "true"}}));
            } else {
                link_order.push_back(create_element(elem::videoscale));
                link_order.push_back(create_element(elem::videoconvert));
                if (USE_CPP_ELEMENTS) {
                    link_order.push_back(create_element(elem::tensor_convert));
                    if (!_model_proc.empty()) { // add tensor_normalize_opencv
                        _model_preproc = _model_proc_provider.parseInputPreproc();
                        if (_model_preproc.size()) {
                            if (_model_preproc.size() > 1) {
                                throw std::runtime_error("Only model-proc with single input layer supported"); // TODO
                            }
                            GstElement *normElem = create_element(elem::tensor_normalize_opencv);
                            link_order.push_back(normElem);
                            GstStructure *params = gst_structure_copy(_model_preproc.begin()->get()->params);
                            g_object_set(G_OBJECT(normElem), dlstreamer::param::params_structure,
                                         static_cast<void *>(params), nullptr);
                        }
                    }
                }
            }
            break;
        case PreProcessBackend::VAAPI_OPENCL:
            if (_inference_region == Region::ROI_LIST) {
                GST_ERROR_OBJECT(_base, "PreProcessBackend::VAAPI_OPENCL on roi-list is not supported yet");
                return false;
            } else {
                link_order.push_back(create_caps(VA_SURFACE_CAPS_FEATURE));
                link_order.push_back(create_element(elem::vaapipostproc));
                link_order.push_back(create_element(elem::video_preproc_vaapi_opencl));
                link_order.push_back(create_element(elem::tensor_normalize_opencl));
            }
            break;
        case PreProcessBackend::IE:
            break;
        default: {
            GST_ERROR_OBJECT(_base, "Unexpected linkage type");
            return false;
        }
        }

        return true;
    }

    GstElement *init_postprocessing() {
        if (!USE_CPP_ELEMENTS) {
            auto postproc = gst_element_factory_make("gvatensortometa", nullptr);
            if (!_model_proc.empty()) {
                GST_INFO_OBJECT(_base, "model-proc property set: %s", _model_proc.c_str());
                g_object_set(G_OBJECT(postproc), "model-proc", _model_proc.c_str(), nullptr);
            }
            if (!_labels_path.empty()) {
                g_object_set(G_OBJECT(postproc), "labels", _labels_path.c_str(), nullptr);
            }
            g_object_set(G_OBJECT(postproc), "converter-type", _converter_type, nullptr);
            return postproc;
        }

        if (_model_proc.empty()) {
            GST_WARNING_OBJECT(_base, "Model-proc is not specified: creating default post-processing");
            auto postproc = gst_element_factory_make("tensor_postproc_detection_output", nullptr);
            if (!postproc) {
                GST_ERROR_OBJECT(_base, "Failed to create post-processing element");
                return nullptr;
            }
            return postproc;
        }

        try {
            _model_postproc = _model_proc_provider.parseOutputPostproc();
            if (_model_postproc.size() != 1) {
                GST_ERROR_OBJECT(_base, "Only single output layer supported"); // TODO
                return nullptr;
            }
            GstStructure *structure = _model_postproc.begin()->second;
            std::string converter = gst_structure_get_string(structure, "converter");
            auto postproc = gst_element_factory_make((elem::tensor_postproc_ + converter).data(), nullptr);
            if (!postproc) {
                GST_ERROR_OBJECT(_base, "Unsupported converter name: %s", converter.data());
                return nullptr;
            }
            g_object_set(G_OBJECT(postproc), dlstreamer::param::params_structure, static_cast<void *>(structure),
                         nullptr);
            return postproc;
        } catch (const std::exception &e) {
            GST_ERROR_OBJECT(_base, "Failed to parse model proc file: %s", Utils::createNestedErrorMsg(e).c_str());
            return nullptr;
        }
    }

    PreProcessBackend get_pre_proc_type(GstCaps *caps) {
        if (_pre_proc_backend != PreProcessBackend::AUTO)
            return _pre_proc_backend;

        CapsFeature incaps_feature = get_caps_feature(caps);
        if (incaps_feature == VA_SURFACE_CAPS_FEATURE) {
            if (_device == "GPU")
                return PreProcessBackend::VAAPI_SURFACE_SHARING;
            else
                return PreProcessBackend::VAAPI;
        } else if (incaps_feature == SYSTEM_MEMORY_CAPS_FEATURE) {
            return PreProcessBackend::GST;
        } else {
            throw std::logic_error("Unknown caps feature");
        }
    }

    void set_converter_type(post_processing::ConverterType type) {
        _converter_type = type;
    }

    void init() {
        // inference
        if (USE_CPP_ELEMENTS) {
            _inference = gst_element_factory_make(elem::tensor_inference_openvino, nullptr);
        } else {
            _inference = gst_element_factory_make("gvatensorinference", nullptr);
        }

        // aggregate
        _aggregate = gst_element_factory_make(elem::meta_aggregate, nullptr);
        gst_bin_add(_base, _aggregate);
        GstPad *aggregate_src_pad = gst_element_get_static_pad(_aggregate, "src");
        assert(aggregate_src_pad && "Expected valid pad from mux");
        GST_DEBUG_OBJECT(_base, "setting target src aggregate_src_pad %" GST_PTR_FORMAT, aggregate_src_pad);
        _srcpad = gst_ghost_pad_new("src", aggregate_src_pad);
        gst_element_add_pad(GST_ELEMENT_CAST(_base), _srcpad);
        gst_object_unref(aggregate_src_pad);

        // tee
        _tee = gst_element_factory_make("tee", nullptr);
        gst_bin_add(_base, _tee);
        GstPad *tee_sink_pad = gst_element_get_static_pad(_tee, "sink");
        assert(tee_sink_pad && "Expected valid pad from tee");
        GST_DEBUG_OBJECT(_base, "setting target sink tee_sink_pad %" GST_PTR_FORMAT, tee_sink_pad);
        _sinkpad = gst_ghost_pad_new("sink", tee_sink_pad);
        gst_element_add_pad(GST_ELEMENT_CAST(_base), _sinkpad);
        gst_pad_set_event_function(_sinkpad, [](GstPad *pad, GstObject *parent, GstEvent *event) {
            return unpack(parent)->sink_event_handler(pad, event);
        });
        gst_pad_set_query_function(_sinkpad, [](GstPad *pad, GstObject *parent, GstQuery *query) {
            return unpack(parent)->sink_query_handler(pad, query);
        });
        gst_object_unref(tee_sink_pad);
    }

    void init_with_legacy_element() {
        if (!USE_LEGACY_ELEMENT)
            return;

        GST_WARNING_OBJECT(_base, "%s", UseMicroElements::deprecation_message.c_str());

        _legacy_inference = GVA_INFERENCE_BIN_GET_CLASS(_base)->create_legacy_element(_base);
        if (!_legacy_inference) {
            GST_ELEMENT_ERROR(_base, CORE, FAILED, ("Failed to create legacy inference element"), ("Init failed"));
            return;
        }
        gst_bin_add(_base, _legacy_inference);

        auto inf_src_pad = gst_element_get_static_pad(_legacy_inference, "src");
        _srcpad = gst_ghost_pad_new("src", inf_src_pad);
        gst_element_add_pad(GST_ELEMENT_CAST(_base), _srcpad);
        g_object_unref(inf_src_pad);

        auto inf_sink_pad = gst_element_get_static_pad(_legacy_inference, "sink");
        _sinkpad = gst_ghost_pad_new("sink", inf_sink_pad);
        gst_element_add_pad(GST_ELEMENT_CAST(_base), _sinkpad);
        g_object_unref(inf_sink_pad);
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
            g_value_set_enum(value, static_cast<gint>(_pre_proc_backend));
            break;
        case PROP_INTERVAL:
            g_value_set_uint(value, _interval);
            break;
        case PROP_REGION:
            g_value_set_enum(value, static_cast<gint>(_inference_region));
            break;
        case PROP_OBJECT_CLASS:
            g_value_set_string(value, _object_class.c_str());
            break;
        case PROP_RESHAPE:
            g_value_set_boolean(value, _reshape);
            break;
        case PROP_RESHAPE_WIDTH:
            g_value_set_uint(value, _reshape_width);
            break;
        case PROP_RESHAPE_HEIGHT:
            g_value_set_uint(value, _reshape_height);
            break;
        case PROP_NO_BLOCK:
            g_value_set_boolean(value, _no_block);
            break;
        case PROP_PRE_PROC_CONFIG:
            g_value_set_string(value, _preprocess_config.c_str());
            break;
        case PROP_DEVICE_EXTENSIONS:
            g_value_set_string(value, _device_extensions.c_str());
            break;
        case PROP_LABELS:
            g_value_set_string(value, _labels_path.c_str());
            break;
        case PROP_CPU_THROUGHPUT_STREAMS:
            g_value_set_uint(value, _cpu_throughput_streams);
            break;
        case PROP_GPU_THROUGHPUT_STREAMS:
            g_value_set_uint(value, _gpu_throughput_streams);
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
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference model property set: %s", _model.c_str());
                g_object_set(G_OBJECT(_inference), "model", _model.c_str(), nullptr);
            }
            break;
        case PROP_IE_CONFIG:
            _ie_config = g_value_get_string(value);
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference ie-config property set: %s", _ie_config.c_str());
                g_object_set(G_OBJECT(_inference), "ie-config", _ie_config.c_str(), nullptr);
            }
            break;
        case PROP_DEVICE:
            _device = g_value_get_string(value);
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference device property set: %s", _device.c_str());
                g_object_set(G_OBJECT(_inference), "device", _device.c_str(), nullptr);
            }
            break;
        case PROP_INSTANCE_ID:
            _instance_id = g_value_get_string(value);
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference instance-id property set: %s", _instance_id.c_str());
                g_object_set(G_OBJECT(_inference), USE_CPP_ELEMENTS ? "shared-instance-id" : "instance-id",
                             _instance_id.c_str(), nullptr);
            }
            break;
        case PROP_NIREQ:
            _nireq = g_value_get_uint(value);
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference nireq property set: %u", _nireq);
                g_object_set(G_OBJECT(_inference), "nireq", _nireq, nullptr);
            }
            break;
        case PROP_BATCH_SIZE:
            _batch_size = g_value_get_uint(value);
            if (_inference) {
                GST_INFO_OBJECT(_base, "tensorinference batch-size property set: %u", _batch_size);
                g_object_set(G_OBJECT(_inference), "batch-size", _batch_size, nullptr);
            }
            break;
        case PROP_MODEL_PROC:
            _model_proc = g_value_get_string(value);
            break;
        case PROP_PRE_PROC_BACKEND:
            _pre_proc_backend = static_cast<PreProcessBackend>(g_value_get_enum(value));
            break;
        case PROP_INTERVAL:
            _interval = g_value_get_uint(value);
            break;
        case PROP_REGION:
            _inference_region = static_cast<Region>(g_value_get_enum(value));
            break;
        case PROP_OBJECT_CLASS:
            _object_class = g_value_get_string(value);
            break;
        case PROP_RESHAPE:
            _reshape = g_value_get_boolean(value);
            break;
        case PROP_RESHAPE_WIDTH:
            _reshape_width = g_value_get_uint(value);
            break;
        case PROP_RESHAPE_HEIGHT:
            _reshape_height = g_value_get_uint(value);
            break;
        case PROP_NO_BLOCK:
            _no_block = g_value_get_boolean(value);
            break;
        case PROP_PRE_PROC_CONFIG:
            _preprocess_config = g_value_get_string(value);
            break;
        case PROP_DEVICE_EXTENSIONS:
            _device_extensions = g_value_get_string(value);
            break;
        case PROP_LABELS:
            _labels_path = g_value_get_string(value);
            break;
        case PROP_CPU_THROUGHPUT_STREAMS:
            _cpu_throughput_streams = g_value_get_uint(value);
            break;
        case PROP_GPU_THROUGHPUT_STREAMS:
            _gpu_throughput_streams = g_value_get_uint(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(_base, prop_id, pspec);
            break;
        }
    }

    gboolean init_properties() {
        if (!_model_proc.empty())
            _model_proc_provider.readJsonFile(_model_proc);
        return TRUE;
    }

    GstStateChangeReturn change_state(GstStateChange transition);

  private:
    GstBin *_base = nullptr;
    GstPad *_srcpad = nullptr;
    GstPad *_sinkpad = nullptr;

    GstElement *_tee = nullptr;
    GstElement *_inference = nullptr;
    GstElement *_aggregate = nullptr;
    GstElement *_legacy_inference = nullptr;

    /* inference */
    std::string _model;
    std::string _ie_config;
    std::string _device = DEFAULT_DEVICE;
    std::string _instance_id;
    guint _nireq = DEFAULT_NIREQ;
    guint _batch_size = DEFAULT_BATCH_SIZE;
    guint _interval = DEFAULT_INTERVAL;
    guint _cpu_throughput_streams = DEFAULT_THROUGHPUT_STREAMS;
    guint _gpu_throughput_streams = DEFAULT_THROUGHPUT_STREAMS;

    /* pre-post-proc */
    std::string _model_proc;
    ModelProcProvider _model_proc_provider;
    std::vector<ModelInputProcessorInfo::Ptr> _model_preproc;
    std::map<std::string, GstStructure *> _model_postproc;
    PreProcessBackend _pre_proc_backend = PreProcessBackend::AUTO;
    post_processing::ConverterType _converter_type = post_processing::ConverterType::RAW;
    Region _inference_region = DEFAULT_INFERENCE_REGION;
    std::string _object_class;
    std::string _labels_path;
    std::string _device_extensions;
    std::string _preprocess_config;
    bool _no_block = DEFAULT_NO_BLOCK;
    bool _reshape = DEFAULT_RESHAPE;
    uint32_t _reshape_width = DEFAULT_RESHAPE_WIDTH;
    uint32_t _reshape_height = DEFAULT_RESHAPE_HEIGHT;

    /* member variables */
    bool _linked = false;
};

#define gva_inference_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_PRIVATE(GvaInferenceBin, gva_inference_bin, GST_TYPE_BIN)

GstStateChangeReturn GvaInferenceBinPrivate::change_state(GstStateChange transition) {
    switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY: {
        if (USE_LEGACY_ELEMENT)
            g_return_val_if_fail(set_legacy_element_properties(), GST_STATE_CHANGE_FAILURE);
        else {
            g_return_val_if_fail(init_properties(), GST_STATE_CHANGE_FAILURE);

            if (_pre_proc_backend != PreProcessBackend::AUTO)
                g_return_val_if_fail(link_elements(_pre_proc_backend), GST_STATE_CHANGE_FAILURE);
        }
    } break;
    default:
        break;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(GST_ELEMENT(_base), transition);
}

bool GvaInferenceBinPrivate::link_elements(PreProcessBackend linkage) {
    if (_linked)
        return true;

    /* determine the link option */
    GST_INFO_OBJECT(_base, "Chosen pre process backend: %d\n", static_cast<int>(linkage));

    // Build pre processing pipeline, create elements by link order list
    std::list<GstElement *> element_list;

    // queue
    element_list.push_back(create_element(elem::queue, {{"max-size-bytes", "0"}}));

    // inference interval and object class filter optionally
    if (_interval > 1) {
        element_list.push_back(create_element(elem::rate_adjust, {{"denominator", std::to_string(_interval)}}));
    }

    // pre-processing elements
    if (!GVA_INFERENCE_BIN_GET_CLASS(_base)->init_preprocessing(_base, linkage, element_list)) {
        GST_ERROR_OBJECT(_base, "Failed to init preprocessing");
        return false;
    }

    if (!USE_CPP_ELEMENTS) {
        element_list.push_back(create_element("gvatensorconverter"));
        element_list.push_back(create_element(elem::queue));
    }

    // inference
    element_list.insert(element_list.end(), {_inference});

    // queue
    if (USE_CPP_ELEMENTS) {
        element_list.insert(element_list.end(), create_element(elem::queue));
    }

    // post-processing element
    GstElement *postproc = GVA_INFERENCE_BIN_GET_CLASS(_base)->init_postprocessing(_base);
    if (!postproc) {
        GST_ERROR_OBJECT(_base, "Failed to init post-processing");
        return false;
    }
    element_list.insert(element_list.end(), {postproc});

    /* add elements to bin */
    for (GstElement *element : element_list) {
        if (!add_element_to_bin(element))
            return false;
    }

    /* link elements */
    auto prev = element_list.begin();
    auto current = prev;
    g_return_val_if_fail(gst_element_link(_tee, *prev), false); // link tee
    while (++current != element_list.end()) {
        if (!*current)
            continue;
        gst_element_unlink(*prev, *current);
        if (!gst_element_link(*prev, *current)) {
            GST_ERROR_OBJECT(_base, "Failed to link elements %s -> %s", GST_ELEMENT_NAME(*prev),
                             GST_ELEMENT_NAME(*current));
            return false;
        }
        GST_DEBUG_OBJECT(_base, "Linked %s -> %s", GST_ELEMENT_NAME(*prev), GST_ELEMENT_NAME(*current));
        prev = current;
    }

    /* link aggregator element */
    g_return_val_if_fail(gst_element_link_pads(element_list.back(), "src", _aggregate, "tensor_%u"), false);

    auto queue = create_element(elem::queue, {{"max-size-bytes", "0"}});
    if (!queue)
        return false;
    add_element_to_bin(queue);

    g_return_val_if_fail(gst_element_link_many(_tee, queue, _aggregate, nullptr), false);
    GST_DEBUG_OBJECT(_base, "Linkage is successful");

    _linked = true;
    return true;
}

void GvaInferenceBin::set_converter_type(post_processing::ConverterType type) {
    impl->set_converter_type(type);
}

PreProcessBackend GvaInferenceBin::get_pre_proc_type(GstCaps *caps) {
    return impl->get_pre_proc_type(caps);
}

static void gva_inference_bin_init(GvaInferenceBin *self) {
    // Initialize of private data
    auto *priv_memory = gva_inference_bin_get_instance_private(self);
    self->impl = new (priv_memory) GvaInferenceBinPrivate(&self->base);

    try {
        // Legacy initialization happens in gobject `constructed` callback, because we use `create_legacy_element`
        // method which may be overriden by derived classes
        // Here we don't have overriden version yet
        if (!USE_LEGACY_ELEMENT)
            self->impl->init();
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(self, CORE, FAILED, ("Failed to init element"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
    }
}

static void gva_inference_bin_finalize(GObject *object) {
    GvaInferenceBin *self = GVA_INFERENCE_BIN(object);
    if (self->impl) {
        // Manually invoke object destruction since it was created via placement-new.
        self->impl->~GvaInferenceBinPrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(parent_class)->finalize(object);
}

static const char *get_batch_size_property_desc() {
    const char *micro_desc =
        "Number of frames batched together for a single inference. Not all models support batching. "
        "Use model optimizer to ensure that the model has batching support.";

    const char *desc = "Number of frames batched together for a single inference. If the batch-size is 0, then it "
                       "will be set by default to be optimal for the device. "
                       "Not all models support batching. Use model optimizer to ensure "
                       "that the model has batching support.";

    return USE_LEGACY_ELEMENT ? desc : micro_desc;
}

static void gva_inference_bin_class_init(GvaInferenceBinClass *klass) {
    GST_DEBUG_CATEGORY_INIT(gva_inference_bin_debug_category, "gvainference", 0, "Debug category of gvainference");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    klass->init_preprocessing = [](GstBin *bin, PreProcessBackend linkage, std::list<GstElement *> &link_order) {
        return GvaInferenceBinPrivate::unpack(bin)->init_preprocessing(linkage, link_order);
    };
    klass->init_postprocessing = [](GstBin *bin) { return GvaInferenceBinPrivate::unpack(bin)->init_postprocessing(); };
    klass->create_legacy_element = [](GstBin *bin) {
        return GvaInferenceBinPrivate::unpack(bin)->create_legacy_element();
    };
    element_class->change_state = [](GstElement *element, GstStateChange transition) {
        return GvaInferenceBinPrivate::unpack(element)->change_state(transition);
    };
    gobject_class->constructed = [](GObject *object) {
        GvaInferenceBinPrivate::unpack(object)->init_with_legacy_element();
    };
    gobject_class->set_property = [](GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
        return GvaInferenceBinPrivate::unpack(object)->set_property(property_id, value, pspec);
    };
    gobject_class->get_property = [](GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
        return GvaInferenceBinPrivate::unpack(object)->get_property(property_id, value, pspec);
    };
    gobject_class->finalize = gva_inference_bin_finalize;

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    constexpr auto prm_flags = static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    /* inference properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL,
        g_param_spec_string("model", "Model", "Path to inference model network file", "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", "",
                            prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Device",
            "Target device for inference. Please see OpenVINO™ Toolkit documentation for list of supported devices.",
            DEFAULT_DEVICE, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_INSTANCE_ID,
        g_param_spec_string(
            "model-instance-id", "Instance ID",
            "Identifier for sharing resources between inference elements of the same type. Elements with "
            "the instance-id will share model and other properties. If not specified, a unique identifier will be "
            "generated.",
            "", prm_flags));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests", MIN_NIREQ,
                                                      MAX_NIREQ, DEFAULT_NIREQ, prm_flags));

    g_object_class_install_property(gobject_class, PROP_BATCH_SIZE,
                                    g_param_spec_uint("batch-size", "Batch Size", get_batch_size_property_desc(),
                                                      MIN_BATCH_SIZE, MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE, prm_flags));

    /* pre-post-proc properties */
    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "Path to JSON file with description of input/output layers pre-processing/post-processing",
                            "", prm_flags));

    g_object_class_install_property(gobject_class, PROP_PRE_PROC_BACKEND,
                                    g_param_spec_enum("pre-process-backend", "Preproc Backend",
                                                      "Preprocessing backend type", GST_TYPE_GVA_INFERENCE_BIN_BACKEND,
                                                      static_cast<int>(PreProcessBackend::AUTO), prm_flags));

    g_object_class_install_property(gobject_class, PROP_INTERVAL,
                                    g_param_spec_uint("inference-interval", "Inference Interval",
                                                      "Run inference for every Nth frame", MIN_INTERVAL, MAX_INTERVAL,
                                                      DEFAULT_INTERVAL, prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_REGION,
        g_param_spec_enum("inference-region", "Inference-Region",
                          "Identifier responsible for the region on which inference will be performed",
                          GST_TYPE_GVA_INFERENCE_BIN_REGION, static_cast<gint>(DEFAULT_INFERENCE_REGION), prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "ObjectClass",
                            "Filter for Region of Interest class label on this element input", "", prm_flags));

    g_object_class_install_property(
        gobject_class, PROP_LABELS,
        g_param_spec_string("labels", "Labels",
                            "Path to file containing model's output layer labels or comma separated list of KEY=VALUE "
                            "pairs where KEY is name of output layer and VALUE is path to labels file. If provided, "
                            "labels from model-proc won't be loaded",
                            "", prm_flags));
    // Legacy properties
    if (USE_LEGACY_ELEMENT) {
        g_object_class_install_property(
            gobject_class, PROP_RESHAPE,
            g_param_spec_boolean("reshape", "Reshape input layer",
                                 "Enable network reshaping.  "
                                 "Use only 'reshape=true' without reshape-width and reshape-height properties "
                                 "if you want to reshape network to the original size of input frames. "
                                 "Note: this feature has a set of limitations. "
                                 "Before use, make sure that your network supports reshaping",
                                 DEFAULT_RESHAPE, prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_RESHAPE_WIDTH,
            g_param_spec_uint("reshape-width", "Width for reshape", "Width to which the network will be reshaped.",
                              MIN_RESHAPE_WIDTH, MAX_RESHAPE_WIDTH, DEFAULT_RESHAPE_WIDTH, prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_RESHAPE_HEIGHT,
            g_param_spec_uint("reshape-height", "Height for reshape", "Height to which the network will be reshaped.",
                              MIN_RESHAPE_HEIGHT, MAX_RESHAPE_HEIGHT, DEFAULT_RESHAPE_HEIGHT, prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_NO_BLOCK,
            g_param_spec_boolean(
                "no-block", "Adaptive inference skipping",
                "(Experimental) Option to help maintain frames per second of incoming stream. Skips inference "
                "on an incoming frame if all inference requests are currently processing outstanding frames",
                DEFAULT_NO_BLOCK, prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_PRE_PROC_CONFIG,
            g_param_spec_string(
                "pre-process-config", "Pre-processing configuration",
                "Comma separated list of KEY=VALUE parameters for image processing pipeline configuration", "",
                prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_DEVICE_EXTENSIONS,
            g_param_spec_string(
                "device-extensions", "ExtensionString",
                "Comma separated list of KEY=VALUE pairs specifying the Inference Engine extension for a device", "",
                prm_flags));

        g_object_class_install_property(
            gobject_class, PROP_CPU_THROUGHPUT_STREAMS,
            g_param_spec_uint("cpu-throughput-streams", "CPU-Throughput-Streams",
                              "Sets the cpu-throughput-streams configuration key for OpenVINO™ Toolkit's "
                              "cpu device plugin. Configuration allows for multiple inference streams "
                              "for better performance. Default mode is auto. See OpenVINO™ Toolkit CPU plugin "
                              "documentation for more details",
                              MIN_THROUGHPUT_STREAMS, MAX_THROUGHPUT_STREAMS, DEFAULT_THROUGHPUT_STREAMS,
                              static_cast<GParamFlags>(prm_flags | G_PARAM_DEPRECATED)));

        g_object_class_install_property(
            gobject_class, PROP_GPU_THROUGHPUT_STREAMS,
            g_param_spec_uint("gpu-throughput-streams", "GPU-Throughput-Streams",
                              "Sets the gpu-throughput-streams configuration key for OpenVINO™ Toolkit's "
                              "gpu device plugin. Configuration allows for multiple inference streams "
                              "for better performance. Default mode is auto. See OpenVINO™ Toolkit GPU plugin "
                              "documentation for more details",
                              MIN_THROUGHPUT_STREAMS, MAX_THROUGHPUT_STREAMS, DEFAULT_THROUGHPUT_STREAMS,
                              static_cast<GParamFlags>(prm_flags | G_PARAM_DEPRECATED)));
    }

    gst_element_class_set_metadata(element_class, GVA_INFERENCE_BIN_NAME, "video", GVA_INFERENCE_BIN_DESCRIPTION,
                                   "Intel Corporation");
}
