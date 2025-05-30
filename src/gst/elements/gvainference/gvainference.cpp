/*******************************************************************************
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "gvainference.h"

#include <assert.h>
#include <list>
#include <memory>
#include <utility>

#include <gst/gstcaps.h>
#include <gst/video/video.h>

#include "gst_logger_sink.h"

#include "dlstreamer/gst/context.h"
#include "dlstreamer/gst/frame.h"
#include "dlstreamer/gst/utils.h"
#include "frame_inference.hpp"
#include "model_proc_provider.h"
#include "utils.h"

enum InferenceRegionType { FULL_FRAME, ROI_LIST };

#define DEFAULT_MODEL ""
#define DEFAULT_MODEL_INSTANCE_ID nullptr
#define DEFAULT_MODEL_PROC nullptr
#define DEFAULT_DEVICE "CPU"
#define DEFAULT_DEVICE_EXTENSIONS ""
#define DEFAULT_PRE_PROC "" // empty = autoselection
#define DEFAULT_INFERENCE_REGION FULL_FRAME
#define DEFAULT_OBJECT_CLASS nullptr
#define DEFAULT_LABELS nullptr

#define DEFAULT_MIN_THRESHOLD 0.
#define DEFAULT_MAX_THRESHOLD 1.
#define DEFAULT_THRESHOLD 0.5

#define DEFAULT_MIN_INFERENCE_INTERVAL 1
#define DEFAULT_MAX_INFERENCE_INTERVAL UINT_MAX
#define DEFAULT_INFERENCE_INTERVAL 1
#define DEFAULT_FIRST_FRAME_NUM 0

#define DEFAULT_RESHAPE FALSE

#define DEFAULT_MIN_BATCH_SIZE 0
#define DEFAULT_MAX_BATCH_SIZE 1024
#define DEFAULT_BATCH_SIZE 0

#define DEFAULT_MIN_RESHAPE_WIDTH 0
#define DEFAULT_MAX_RESHAPE_WIDTH UINT_MAX
#define DEFAULT_RESHAPE_WIDTH 0

#define DEFAULT_MIN_RESHAPE_HEIGHT 0
#define DEFAULT_MAX_RESHAPE_HEIGHT UINT_MAX
#define DEFAULT_RESHAPE_HEIGHT 0

#define DEFAULT_NO_BLOCK FALSE

#define DEFAULT_MIN_NIREQ 0
#define DEFAULT_MAX_NIREQ 1024
#define DEFAULT_NIREQ 0

#define DEFAULT_CPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MIN_CPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MAX_CPU_THROUGHPUT_STREAMS UINT_MAX

#define DEFAULT_GPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MIN_GPU_THROUGHPUT_STREAMS 0
#define DEFAULT_MAX_GPU_THROUGHPUT_STREAMS UINT_MAX

GType enum_gva_inference_region_type(void) {
    static GType gva_inference_region = 0;
    static const GEnumValue inference_region_types[] = {{FULL_FRAME, "Perform inference for full frame", "full-frame"},
                                                        {ROI_LIST, "Perform inference for roi list", "roi-list"},
                                                        {0, nullptr, nullptr}};

    if (!gva_inference_region) {
        gva_inference_region = g_enum_register_static("InferenceRegionType2", inference_region_types);
    }
    return gva_inference_region;
}

enum {
    PROP_0,
    PROP_MODEL,
    PROP_DEVICE,
    PROP_INFERENCE_INTERVAL,
    PROP_RESHAPE,
    PROP_BATCH_SIZE,
    PROP_RESHAPE_WIDTH,
    PROP_RESHAPE_HEIGHT,
    PROP_NO_BLOCK,
    PROP_NIREQ,
    PROP_MODEL_INSTANCE_ID,
    PROP_PRE_PROC_BACKEND,
    PROP_MODEL_PROC,
    PROP_CPU_THROUGHPUT_STREAMS,
    PROP_GPU_THROUGHPUT_STREAMS,
    PROP_IE_CONFIG,
    PROP_PRE_PROC_CONFIG,
    PROP_DEVICE_EXTENSIONS,
    PROP_INFERENCE_REGION,
    PROP_OBJECT_CLASS,
    PROP_LABELS,
    PROP_LABELS_FILE,
    PROP_SCALE_METHOD
};

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR, NV12, I420 }") "; "

#ifdef ENABLE_VAAPI
#define VASURFACE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VASurface", "{ NV12 }") "; "
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ RGBA, I420 }") "; "
#else
#define VASURFACE_CAPS
#define DMA_BUFFER_CAPS
#endif

#define GVA_INFERENCE_CAPS SYSTEM_MEM_CAPS VASURFACE_CAPS DMA_BUFFER_CAPS

GST_DEBUG_CATEGORY_STATIC(gva_inference_debug);
#define GST_CAT_DEFAULT gva_inference_debug

namespace dls = dlstreamer;

namespace dlstreamer {

// TODO: Move to common place
template <auto>
struct Callback;

template <class T, class Ret, class... Args, Ret (T::*mem_fn)(Args...)>
struct Callback<mem_fn> {
    static Ret fn(GObject *obj, Args... args) {
        return (T::unpack(obj)->*mem_fn)(std::forward<Args>(args)...);
    }

    static Ret fn(GstBaseTransform *obj, Args... args) {
        return (T::unpack(obj)->*mem_fn)(std::forward<Args>(args)...);
    }
};

class GstFrameEx : public GSTFrame {
  public:
    using GSTFrame::GSTFrame;

    // Releases ownership of the buffer
    GstBuffer *release_gst_buffer() {
        if (!_take_ownership)
            return nullptr;
        _take_ownership = false;
        return _gst_buffer;
    }

    bool is_ready() const {
        return _ready;
    }

    void set_ready(bool ready = true) {
        _ready = ready;
    }

  protected:
    bool _ready = false;
};

using GstFrameExPtr = std::shared_ptr<GstFrameEx>;

} // namespace dlstreamer

class GvaInferencePrivate final {
    using FramesEventsList = std::list<dls::GstFrameExPtr>;

  public:
    static GvaInferencePrivate *unpack(GstBaseTransform *base) noexcept {
        return GVA_INFERENCE_CAST(base)->impl;
    }
    static GvaInferencePrivate *unpack(GObject *object) noexcept {
        return unpack(GST_BASE_TRANSFORM(object));
    }

    GvaInferencePrivate(GstBaseTransform *parent, gpointer parent_class) noexcept
        : _base(parent), _base_class(parent_class),
          _gst_context(std::make_shared<dls::GSTContext>(GST_ELEMENT(_base))) {
        try {
            _logger = dls::log::init_logger(GST_CAT_DEFAULT, G_OBJECT(_base));
        } catch (const std::runtime_error &e) {
            GST_ERROR("Failed to initialize logger: %s", e.what());
        } catch (...) {
            GST_ERROR("Unknown exception occurred while initializing logger");
        }
    }

    gboolean start() {
        GST_DEBUG_OBJECT(_base, "start");
        return verify_properties();
    }

    gboolean stop() {
        GST_DEBUG_OBJECT(_base, "stop, in/out buffers: %lu/%lu", _counters.in_buffers, _counters.out_buffers);
        flush_inference();
        return true;
    }

    gboolean set_caps(GstCaps *incaps, GstCaps *outcaps) {
        try {
            GST_DEBUG_OBJECT(_base, "in={%" GST_PTR_FORMAT "} --- out={%" GST_PTR_FORMAT "}", incaps, outcaps);
            GST_DEBUG_OBJECT(_base, "active input info: {%s}", dls::frame_info_to_string(_input_info).c_str());

            dls::FrameInfo in_info = dls::gst_caps_to_frame_info(incaps);
            if (_inference && _input_info == in_info) {
                // We alredy have an inference model instance.
                return true;
            }

            // New caps or caps has changed.
            _input_info = std::move(in_info);
            GST_DEBUG_OBJECT(_base, "input info set: {%s}", dls::frame_info_to_string(_input_info).c_str());
            if (!gst_video_info_from_caps(&_input_video_info, incaps))
                GST_WARNING_OBJECT(_base, "couldn't convert input caps {%" GST_PTR_FORMAT "} to video info", incaps);

            acquire_inference_isntance();
            GST_DEBUG_OBJECT(_base, "acquired inference instance: %p", _inference.get());
        } catch (const std::exception &e) {
            GST_ELEMENT_ERROR(_base, LIBRARY, FAILED, ("caught an exception when processing set caps"),
                              ("%s", Utils::createNestedErrorMsg(e).c_str()));
            return false;
        }

        return true;
    }

    gboolean sink_event(GstEvent *event) {
        GST_DEBUG_OBJECT(_base, "sink event, type=%#x", event->type);
        if (event->type == GST_EVENT_EOS || event->type == GST_EVENT_FLUSH_STOP) {
            GST_INFO_OBJECT(_base, "got eos or flush event, type=%#x", event->type);
            flush_inference();
        }
        return GST_BASE_TRANSFORM_CLASS(_base_class)->sink_event(_base, event);
    }

    void get_property(guint property_id, GValue *value, GParamSpec *pspec) {
        (void)property_id;
        (void)value;
        (void)pspec;
    }

    void set_property(guint property_id, const GValue *value, GParamSpec *pspec) {
        (void)pspec;

        switch (property_id) {
        case PROP_MODEL:
            _properties.model_path = g_value_get_string(value);
            break;
        case PROP_DEVICE:
            _properties.device = g_value_get_string(value);
            break;
        case PROP_BATCH_SIZE:
            _properties.batch_size = g_value_get_uint(value);
            break;
        case PROP_MODEL_PROC:
            _properties.model_proc_path = g_value_get_string(value);
            break;
        case PROP_NIREQ:
            _properties.nireq = g_value_get_uint(value);
            break;
        case PROP_IE_CONFIG:
            _properties.ie_config = g_value_get_string(value);
            break;
        case PROP_PRE_PROC_BACKEND:
            _properties.preprocessing_backend = g_value_get_string(value);
            break;
        default:
            // FIXME: once all propreties are implemented
            // G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
        }
    }

    GstFlowReturn generate_output(GstBuffer **outbuf) {
        GST_LOG_OBJECT(_base, "generate_output");
        // Set outbuf to null since inference will be completed in async way
        *outbuf = nullptr;

        // Take ownership of input buffer
        GstBuffer *buf = _base->queued_buf;
        _base->queued_buf = nullptr;
        buf = gst_buffer_make_writable(buf);
        _counters.in_buffers++;

        try {
            // TODO: use ctor with GstVideoInfo?
            // auto frame = std::make_shared<dls::GstFrameEx>(buf, _input_info, true);
            auto frame = std::make_shared<dls::GstFrameEx>(buf, &_input_video_info, nullptr, true);
            put_frame_to_queue(frame);
            _inference->run_async(frame, [this](dls::FramePtr frame) { on_frame_ready(std::move(frame)); });

        } catch (const std::exception &e) {
            GST_ELEMENT_ERROR(_base, LIBRARY, FAILED, ("caught an exception when requesting start of frame inference"),
                              ("%s", Utils::createNestedErrorMsg(e).c_str()));
        }

        // Buffer will be pushed once inference is completed
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    GstFlowReturn transform_ip(GstBuffer *) {
        // This should not be called since generate_output is overriden.
        // It's provided only for internal logic of BaseTransform
        GST_LOG_OBJECT(_base, "transform_ip");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

  private:
    bool verify_properties() const {
        return true;
    }

    void acquire_inference_isntance() {
        try {
            // Need to release current instance of inference if any
            flush_inference();
            _inference.reset();

            dls::FrameInferenceParams params;
            prepeare_inference_params(params);

            // TODO: shared instance
            _inference =
                std::make_shared<dls::FrameInference>(params, _gst_context, _input_info.memory_type, _input_info);
        } catch (...) {
            std::throw_with_nested(std::runtime_error("failed to acquire inference instance"));
        }
    }

    void prepeare_inference_params(dls::FrameInferenceParams &params) const {
        params.logger_name = _logger->name();

        params.model_path = _properties.model_path;
        params.device = _properties.device;
        params.batch_size = _properties.batch_size;
        params.nireq = _properties.nireq;
        // FIXME
        params.ov_config_str = _properties.ie_config;
        params.ov_config_map = Utils::stringToMap(_properties.ie_config);

        if (auto ppb = dls::FrameInferenceParams::preprocess_backend_from_string(_properties.preprocessing_backend)) {
            params.preprocess_be = *ppb;
        } else {
            throw std::runtime_error(
                fmt::format("invalid pre-process-backend value: '{}'", _properties.preprocessing_backend));
        }

        read_params_from_model_proc(params);
    }

    void read_params_from_model_proc(dls::FrameInferenceParams &params) const {
        const auto &path = _properties.model_proc_path;
        if (path.empty())
            return;

        if (!Utils::fileExists(path))
            throw std::runtime_error(fmt::format("model-proc file '{}' doesn't exist", path));

        // FIXME: move to model proce provider?
        const constexpr size_t MAX_MODEL_PROC_SIZE = 10 * 1024 * 1024; // 10 Mb
        if (!Utils::CheckFileSize(path, MAX_MODEL_PROC_SIZE))
            throw std::runtime_error(
                fmt::format("model-proc file '{}' size exceeds the allowable size (10 MB).", path));

        ModelProcProvider mpp;
        mpp.readJsonFile(path);
        params.preprocessing_params = mpp.parseInputPreproc();
        auto out_pp = mpp.parseOutputPostproc();
        auto inserter_it = std::inserter(params.postprocessing_params, params.postprocessing_params.end());
        std::transform(out_pp.begin(), out_pp.end(), inserter_it, [](auto &pair) {
            // pair.second is GstStructure*
            return std::make_pair(pair.first, std::make_shared<dls::GSTDictionary>(pair.second));
        });
        GST_DEBUG_OBJECT(_base, "successfully read model-proc file='%s'", path.c_str());
    }

    void on_frame_ready(dls::FramePtr frame) {
        GST_LOG_OBJECT(_base, "frame ready, ptr=%p", frame.get());
        auto gst_frame = std::dynamic_pointer_cast<dls::GstFrameEx>(frame);
        if (!gst_frame) {
            GST_ERROR_OBJECT(_base, "invalid frame type was provided, ptr=%p", frame.get());
            return;
        }

        gst_frame->set_ready();
        push_ready_frames();
    }

    void push_ready_frames() {
        FramesEventsList ready_list = get_ready_frames();

        size_t count = 0;
        while (!ready_list.empty()) {
            count++;
            // Take ownership of the buffer
            GstBuffer *buf = ready_list.front()->release_gst_buffer();
            ready_list.pop_front();
            GstFlowReturn ret = gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(_base), buf);
            if (ret != GST_FLOW_OK)
                GST_WARNING_OBJECT(_base, "gst_pad_push returned status: %d", ret);
        }
        GST_LOG_OBJECT(_base, "pushed %lu ready frames", count);
        _counters.out_buffers += count;
    }

    void flush_inference() {
        if (!_inference)
            return;

        try {
            GST_DEBUG_OBJECT(_base, "flushing inference instance...");
            _inference->flush();
        } catch (const std::exception &e) {
            GST_ELEMENT_ERROR(_base, LIBRARY, FAILED, ("caught an exception during inference flush operation"),
                              ("%s", Utils::createNestedErrorMsg(e).c_str()));
        }
    }

    void put_frame_to_queue(dls::GstFrameExPtr frame) {
        std::lock_guard<std::mutex> lock(_queue_mutex);
        _queue.emplace_back(std::move(frame));
    }

    FramesEventsList get_ready_frames() {
        FramesEventsList l;
        std::lock_guard<std::mutex> lock(_queue_mutex);
        auto it =
            std::find_if(_queue.begin(), _queue.end(), [](const dls::GstFrameExPtr &f) { return !f->is_ready(); });
        l.splice(l.begin(), _queue, _queue.begin(), it);
        return l;
    }

  private:
    GstBaseTransform *_base;
    gpointer _base_class;

    struct {
        std::string model_path;
        std::string device = DEFAULT_DEVICE;
        std::string model_proc_path;
        std::string ie_config;
        std::string preprocessing_backend;
        guint batch_size = 0;
        guint nireq = 0;
    } _properties;

    // Queue to keep order of buffers and events
    // TODO: events
    FramesEventsList _queue;
    std::mutex _queue_mutex;

    std::shared_ptr<dls::FrameInference> _inference;

    dls::FrameInfo _input_info;
    GstVideoInfo _input_video_info = {};

    std::shared_ptr<dls::GSTContext> _gst_context;
    std::shared_ptr<spdlog::logger> _logger;

    struct {
        size_t in_buffers = 0;
        size_t out_buffers = 0;
    } _counters;
};

// Define type after private data
G_DEFINE_TYPE_WITH_PRIVATE(GvaInference, gva_inference, GST_TYPE_BASE_TRANSFORM);

void gva_inference_init(GvaInference *self) {
    GST_DEBUG_OBJECT(self, "gvainference init!");

    // Intialization of private data
    auto *priv_memory = gva_inference_get_instance_private(self);
    // This won't be converted to shared ptr because of memory placement
    self->impl = new (priv_memory) GvaInferencePrivate(GST_BASE_TRANSFORM(self), gva_inference_parent_class);

    // Optional. Set in-place
    gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), true);
}

void gva_inference_finalize(GObject *object) {
    GvaInference *self = GVA_INFERENCE_CAST(object);
    GST_DEBUG_OBJECT(self, "gvainference finalize!");
    assert(self->impl);

    if (self->impl) {
        // Manually invoke object destruction since it was created via placement-new.
        self->impl->~GvaInferencePrivate();
        self->impl = nullptr;
    }

    G_OBJECT_CLASS(gva_inference_parent_class)->finalize(object);
}

void gva_inference_class_init(GvaInferenceClass *klass) {
    using namespace dls;
    GST_DEBUG_CATEGORY_INIT(gva_inference_debug, "gvainference", 0, "Debug category for gvainference element");

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_INFERENCE_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(GVA_INFERENCE_CAPS)));

    gst_element_class_set_static_metadata(
        element_class, "Generic full-frame inference (generates GstGVATensorMeta)", "Video",
        "Runs deep learning inference using any model with an RGB or BGR input.", "Intel Corporation");

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->start = Callback<&GvaInferencePrivate::start>::fn;
    base_transform_class->stop = Callback<&GvaInferencePrivate::stop>::fn;
    base_transform_class->set_caps = Callback<&GvaInferencePrivate::set_caps>::fn;
    base_transform_class->sink_event = Callback<&GvaInferencePrivate::sink_event>::fn;
    base_transform_class->generate_output = Callback<&GvaInferencePrivate::generate_output>::fn;
    base_transform_class->transform_ip = Callback<&GvaInferencePrivate::transform_ip>::fn;

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gva_inference_finalize;
    gobject_class->set_property = Callback<&GvaInferencePrivate::set_property>::fn;
    gobject_class->get_property = Callback<&GvaInferencePrivate::get_property>::fn;

    constexpr auto param_flags =
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY);

    g_object_class_install_property(
        gobject_class, PROP_MODEL,
        g_param_spec_string("model", "Model", "Path to inference model network file", DEFAULT_MODEL, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_INSTANCE_ID,
        g_param_spec_string(
            "model-instance-id", "Model Instance Id",
            "Identifier for sharing a loaded model instance between elements of the same type. Elements with the "
            "same model-instance-id will share all model and inference engine related properties",
            DEFAULT_MODEL_INSTANCE_ID, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_PRE_PROC_BACKEND,
        g_param_spec_string("pre-process-backend", "Pre-processing method",
                            "Select a pre-processing method (color conversion, resize and crop), "
                            "one of 'ie', 'opencv', 'vaapi', 'vaapi-surface-sharing'. If not set, it will be selected "
                            "automatically: 'vaapi' for VASurface and DMABuf, 'ie' for SYSTEM memory.",
                            DEFAULT_PRE_PROC, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_MODEL_PROC,
        g_param_spec_string("model-proc", "Model preproc and postproc",
                            "Path to JSON file with description of input/output layers pre-processing/post-processing",
                            DEFAULT_MODEL_PROC, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE,
        g_param_spec_string(
            "device", "Device",
            "Target device for inference. Please see OpenVINO™ Toolkit documentation for list of supported devices.",
            DEFAULT_DEVICE, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_BATCH_SIZE,
        g_param_spec_uint("batch-size", "Batch size",
                          "Number of frames batched together for a single inference. If the batch-size is 0, then it "
                          "will be set by default to be optimal for the device."
                          "Not all models support batching. Use model optimizer to ensure "
                          "that the model has batching support.",
                          DEFAULT_MIN_BATCH_SIZE, DEFAULT_MAX_BATCH_SIZE, DEFAULT_BATCH_SIZE, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_INTERVAL,
        g_param_spec_uint("inference-interval", "Inference Interval",
                          "Interval between inference requests. An interval of 1 (Default) performs inference on "
                          "every frame. An interval of 2 performs inference on every other frame. An interval of N "
                          "performs inference on every Nth frame.",
                          DEFAULT_MIN_INFERENCE_INTERVAL, DEFAULT_MAX_INFERENCE_INTERVAL, DEFAULT_INFERENCE_INTERVAL,
                          param_flags));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE,
        g_param_spec_boolean("reshape", "Reshape input layer",
                             "If true, model input layer will be reshaped to resolution of input frames "
                             "(no resize operation before inference). "
                             "Note: this feature has limitations, not all network supports reshaping.",
                             DEFAULT_RESHAPE, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE_WIDTH,
        g_param_spec_uint("reshape-width", "Width for reshape", "Width to which the network will be reshaped.",
                          DEFAULT_MIN_RESHAPE_WIDTH, DEFAULT_MAX_RESHAPE_WIDTH, DEFAULT_RESHAPE_WIDTH, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_RESHAPE_HEIGHT,
        g_param_spec_uint("reshape-height", "Height for reshape", "Height to which the network will be reshaped.",
                          DEFAULT_MIN_RESHAPE_HEIGHT, DEFAULT_MAX_RESHAPE_HEIGHT, DEFAULT_RESHAPE_HEIGHT, param_flags));

    g_object_class_install_property(gobject_class, PROP_NIREQ,
                                    g_param_spec_uint("nireq", "NIReq", "Number of inference requests",
                                                      DEFAULT_MIN_NIREQ, DEFAULT_MAX_NIREQ, DEFAULT_NIREQ,
                                                      param_flags));

    g_object_class_install_property(
        gobject_class, PROP_IE_CONFIG,
        g_param_spec_string("ie-config", "Inference-Engine-Config",
                            "Comma separated list of KEY=VALUE parameters for Inference Engine configuration. "
                            "See OpenVINO™ Toolkit documentation for available parameters",
                            "", param_flags));

    g_object_class_install_property(
        gobject_class, PROP_PRE_PROC_CONFIG,
        g_param_spec_string("pre-process-config", "Pre-processing configuration",
                            "Comma separated list of KEY=VALUE parameters for image processing pipeline configuration",
                            "", param_flags));

    g_object_class_install_property(
        gobject_class, PROP_DEVICE_EXTENSIONS,
        g_param_spec_string(
            "device-extensions", "ExtensionString",
            "Comma separated list of KEY=VALUE pairs specifying the Inference Engine extension for a device",
            DEFAULT_DEVICE_EXTENSIONS, static_cast<GParamFlags>(param_flags | G_PARAM_DEPRECATED)));

    g_object_class_install_property(
        gobject_class, PROP_INFERENCE_REGION,
        g_param_spec_enum("inference-region", "Inference-Region",
                          "Identifier responsible for the region on which inference will be performed",
                          enum_gva_inference_region_type(), DEFAULT_INFERENCE_REGION, param_flags));

    g_object_class_install_property(
        gobject_class, PROP_OBJECT_CLASS,
        g_param_spec_string("object-class", "ObjectClass",
                            "Filter for Region of Interest class label on this element input", DEFAULT_OBJECT_CLASS,
                            (GParamFlags)(param_flags)));

    g_object_class_install_property(
        gobject_class, PROP_LABELS,
        g_param_spec_string("labels", "Labels",
                            "Array of object classes. "
                            "It could be set as the following example: labels=<label1,label2,label3>",
                            DEFAULT_LABELS, param_flags));

    g_object_class_install_property(gobject_class, PROP_LABELS_FILE,
                                    g_param_spec_string("labels-file", "Labels-file",
                                                        "Path to .txt file containing object classes (one per line)",
                                                        DEFAULT_LABELS, param_flags));

    g_object_class_install_property(gobject_class, PROP_SCALE_METHOD,
                                    g_param_spec_string("scale-method", "scale-method",
                                                        "Scale method to use in pre-preprocessing before inference."
                                                        " Only default and scale-method=fast (VAAPI based) supported "
                                                        "in this element",
                                                        nullptr, param_flags));
}

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvainference", GST_RANK_NONE, gva_inference_get_type()))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvainference, PRODUCT_FULL_NAME " gvainference element",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
