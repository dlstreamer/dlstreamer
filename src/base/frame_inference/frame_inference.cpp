/*******************************************************************************
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer_logger.h"

#include "frame_inference.hpp"

#include "dlstreamer/base/context.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"

#include "dlstreamer/cpu/elements/tensor_postproc_add_params.h"
#include "dlstreamer/cpu/elements/tensor_postproc_detection.h"
#include "dlstreamer/cpu/elements/tensor_postproc_label.h"
#include "dlstreamer/cpu/elements/tensor_postproc_text.h"
#include "dlstreamer/cpu/elements/tensor_postproc_yolo.h"

#include "openvino.hpp"

#include "dlstreamer/element.h"

template <>
struct fmt::formatter<dlstreamer::PreprocessBackend> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.

    auto format(dlstreamer::PreprocessBackend pb, format_context &ctx) const {
        return formatter<string_view>::format(to_string_view(pb), ctx);
    }

    string_view to_string_view(dlstreamer::PreprocessBackend pb) const {
        using namespace dlstreamer;
        switch (pb) {
        case PreprocessBackend::Auto:
            return "auto";
        case PreprocessBackend::OpenVino:
            return "ie";
        case PreprocessBackend::VaApi:
            return "vaapi";
        case PreprocessBackend::VaApiSurfaceSharing:
            return "vaapi-surface-sharing";
        case PreprocessBackend::OpenCv:
            return "opencv";
        default:
            return "<unknown>";
        }
    }
};

using namespace dlstreamer;

std::optional<PreprocessBackend> FrameInferenceParams::preprocess_backend_from_string(const std::string &ppb_name) {
    static constexpr std::pair<std::string_view, PreprocessBackend> string_to_backend_map[]{
        {"", PreprocessBackend::Auto},
        {"auto", PreprocessBackend::Auto},
        {"ie", PreprocessBackend::OpenVino},
        {"vaapi", PreprocessBackend::VaApi},
        {"vaapi-surface-sharing", PreprocessBackend::VaApiSurfaceSharing},
        {"opencv", PreprocessBackend::OpenCv}};

    for (auto &elem : string_to_backend_map) {
        if (ppb_name == elem.first)
            return elem.second;
    }

    return {};
}

namespace {

void log_params(const FrameInferenceParams &params, spdlog::logger &log) {
    log.info("FrameInference parameters: model={}, device={}, batch-size={}, preprocess-backend={}", params.model_path,
             params.device, params.batch_size, params.preprocess_be);
}

// FIXME: move to transform.h?
std::unique_ptr<TransformInplace> create_transform_inplace(const ElementDesc &desc, DictionaryCPtr params,
                                                           const ContextPtr &app_context) {
    Element *element = desc.create(params, app_context);
    auto *transform = dynamic_cast<TransformInplace *>(element);
    if (!transform) {
        delete element;
        throw std::runtime_error(fmt::format("couldn't downcast to TransformInplace type for element {}", desc.name));
    }
    return std::unique_ptr<TransformInplace>(transform);
}

auto create_transform_inplace(const ElementDesc &desc, const AnyMap &params = AnyMap(),
                              const ContextPtr &app_context = nullptr) {
    return create_transform_inplace(desc, std::make_shared<BaseDictionary>(params), app_context);
}

ElementDesc *get_postproc_element_desc(std::string_view converter_name) {
    static ElementDesc *elements[] = {
        &tensor_postproc_detection,
        &tensor_postproc_yolo,
        &tensor_postproc_add_params,
        // post-processing for object classification
        &tensor_postproc_label,
        &tensor_postproc_text,
    };

    auto it = std::find_if(std::begin(elements), std::end(elements),
                           [&](auto desc) { return desc->name.find(converter_name) != desc->name.npos; });
    if (it != std::end(elements))
        return {*it};
    return nullptr;
} // namespace

std::string_view adjust_postproc_converter_name(std::string_view name) {
    // FIXME: yolo version
    static std::map<std::string_view, std::string_view> replacements = {{"detection_output", "detection"},
                                                                        {"boxes_labels", "detection"},
                                                                        {"boxes", "detection"},
                                                                        {"yolo_v3", "yolo"},
                                                                        {"yolo_v4", "yolo"},
                                                                        {"yolo_v5", "yolo"},
                                                                        {"keypoints_openpose", "human_pose"}};

    auto it = replacements.find(name);
    if (it != replacements.end())
        return it->second;
    return name;
}

} // namespace

// #define FAKE_OV

FrameInference::FrameInference(const FrameInferenceParams &params, ContextPtr app_context, MemoryType in_memory_type,
                               FrameInfo &input_info)
    : _app_context(std::move(app_context)), _log(log::get_or_nullsink(params.logger_name)),
      _input_memory_type(in_memory_type), _input_info(input_info) {
    auto task = itt::Task("frame_inference:FrameInference");
    log_params(params, *_log);

    ContextPtr interm_context = std::make_shared<BaseContext>(_input_memory_type);
    _input_mapper = _app_context->get_mapper(_app_context, interm_context);

    init_inference_backend(params);

    init_post_processing(params);

#ifdef FAKE_OV
    // STUB
    _stub.ov_completion_thread = std::thread(&FrameInference::fake_ov_worker, this);
#endif
}

FrameInference::~FrameInference() {
#ifdef FAKE_OV
    _stub.thread_running = false;
    _stub.ov_signal.notify_all();
    _stub.ov_completion_thread.join();
#endif
}

void FrameInference::run_async(FramePtr frame, FrameReadyCallback ready_cb) {
    auto task = itt::Task("frame_inference:FrameInference:run_async");
    if (!ready_cb)
        throw std::invalid_argument("ready_cb cannot be empty");

    {
        std::lock_guard<std::mutex> frames_lock(_frames_list_mutex);
        _frames_list.emplace_back(frame, std::move(ready_cb));
    }

#ifdef FAKE_OV
    auto f = _input_mapper->map(frame);
    //_log->debug("input/mapped frame memory type {}/{}", memory_type_to_string(frame->memory_type()),
    //           memory_type_to_string(f->memory_type()));
    // STUB - Start Inference
    fake_start_inference_internal(frame);
#else
    // Actial code goes here
    auto f = _input_mapper->map(frame);
    start_inference_internal(f);
#endif
}

void FrameInference::flush() {
    auto task = itt::Task("frame_inference:FrameInference:flush");
#ifdef FAKE_OV
    // STUB
    _stub.flush = true;
    _stub.ov_signal.notify_one();
    while (_stub.flush) {
        // BAD! BUSY LOOP
    }
#else
    _ov_backend->flush();
#endif
}

void FrameInference::init_inference_backend(const FrameInferenceParams &params) {
    auto task = itt::Task("frame_inference:FrameInference:init_inference_backend");
    auto backend_params = std::make_shared<BaseDictionary>();

    backend_params->set("model", params.model_path);
    backend_params->set("device", params.device);
    backend_params->set("config", params.ov_config_str);
    backend_params->set("batch-size", int(params.batch_size));
    backend_params->set("nireq", int(params.nireq));

    backend_params->set(param::logger_name, params.logger_name);

    _ov_backend = std::make_unique<OpenVinoBackend>(backend_params, _input_info);
    _log->info("initialized inference backend, model input={} output={}",
               frame_info_to_string(_ov_backend->get_model_input()),
               frame_info_to_string(_ov_backend->get_model_output()));
}

void FrameInference::init_post_processing(const FrameInferenceParams &params) {
    auto task = itt::Task("frame_inference:FrameInference:init_post_processing");
    // Inference backend should be created before this call
    assert(_ov_backend);

    std::string_view pp_name;

    if (!params.postprocessing_params.empty()) { // Go through all post-processors in model-proc file

        if (params.postprocessing_params.size() > 1)
            throw std::runtime_error("!!! unimplemented !!!");

        for (auto &pp_item : params.postprocessing_params) {
            auto converter = pp_item.second->get("converter", std::string{});
            if (converter.empty()) {
                SPDLOG_LOGGER_WARN(_log, "converter name is not set");
                break;
            }
            auto adjusted_converter = adjust_postproc_converter_name(converter);
            SPDLOG_LOGGER_DEBUG(_log, "converter name: '{}', adjusted name: '{}'", converter, adjusted_converter);

            ElementDesc *pp_desc = get_postproc_element_desc(adjusted_converter);
            if (!pp_desc) {
                SPDLOG_LOGGER_ERROR(_log, "unsupported post-processing converter: '{}' (adjusted: '{}')", converter,
                                    adjusted_converter);
                break;
            }

            pp_item.second->set(param::logger_name, params.logger_name);
            _post_processing_elem = create_transform_inplace(*pp_desc, pp_item.second, nullptr);
            pp_name = pp_desc->name;
        }
    } else { // default postprocess element
        // FIXME: better default
        // FIXME: should we use dlstreamer_elements[]?
        // FIXME: pass parameters like labels, threshold, labels-file, etc.
        // FIXME: context
        AnyMap postproc_params;
        postproc_params.emplace(param::logger_name, params.logger_name);

        _post_processing_elem = create_transform_inplace(tensor_postproc_detection, postproc_params, nullptr);
        pp_name = tensor_postproc_detection.name;
    }

    if (!_post_processing_elem) {
        SPDLOG_LOGGER_WARN(_log, "post-processing element wasn't created");
        return;
    }

    // Element initialization
    _post_processing_elem->init();
    _post_processing_elem->set_info(_ov_backend->get_model_output());

    SPDLOG_LOGGER_DEBUG(_log, "initialized post-processing element, name={}", pp_name);
}

void FrameInference::start_inference_internal(FramePtr frame) {
    // TODO
    auto task = itt::Task("frame_inference:FrameInference:start_inference_internal");
    _ov_backend->infer_async({frame}, [this](FramePtr frame, TensorVector tensors) {
        try {
            on_inference_complete(std::move(frame), std::move(tensors));
        } catch (std::exception &e) {
            _log->critical("caught an exception during inference post-processing: {}", e.what());
        }
    });
}

void FrameInference::on_inference_complete(FramePtr frame, TensorVector output_tensors) {
    // This code path is work in progress and not active in DLStreamer architecture 1.0
    assert(false);

    auto task = itt::Task("frame_inference:FrameInference:on_inference_complete");
    SPDLOG_LOGGER_TRACE(_log, "on inference complete callback, frame={}", static_cast<void *>(frame.get()));

    FrameReadyCallback frame_ready_cb;
    {
        auto task = itt::Task("frame_inference:FrameInference:frame_ready_cb");
        // Extract frame and callback for it from list
        std::lock_guard<std::mutex> lock(_frames_list_mutex);
        auto it = std::find_if(_frames_list.begin(), _frames_list.end(), [&](const ListEntry &item) {
            return item.frame == frame || item.frame == frame->parent();
        });

        if (it == _frames_list.end()) {
            _log->error("couldn't find frame {} in internal queue", static_cast<void *>(frame.get()));
            return;
        }
        // FIXME: find better way
        frame = it->frame;

        frame_ready_cb = it->ready_callback;
        _frames_list.erase(it);
    }

    // TODO
    postprocess(frame, std::move(output_tensors));

    assert(frame_ready_cb);
    frame_ready_cb(std::move(frame));
}

class PostProcFrame : public BaseFrame {
    FramePtr _original_frame;

  public:
    PostProcFrame(FramePtr original_frame, TensorVector output_tensors)
        : BaseFrame(MediaType::Tensors, 0, MemoryType::CPU), _original_frame(std::move(original_frame)) {
        auto task = itt::Task("frame_inference:PostProcFrame");
        _tensors = std::move(output_tensors);
    }

    Metadata &metadata() override {
        // Return metadata object of original frame so all meta is added to original frame
        auto task = itt::Task("frame_inference:metadata");
        return _original_frame->metadata();
    }
};

void FrameInference::postprocess(FramePtr frame, TensorVector output_tensors) {
    auto task = itt::Task("frame_inference:FrameInference:postprocess");
    if (!_post_processing_elem)
        return;

    auto pp_frame = std::make_shared<PostProcFrame>(std::move(frame), std::move(output_tensors));

    auto model_info = add_metadata<ModelInfoMetadata>(*pp_frame);
    model_info.set_model_name(_ov_backend->get_model_name());
    model_info.set_info("input", _ov_backend->get_model_input());
    model_info.set_info("output", _ov_backend->get_model_output());
    model_info.set_layer_names("input", _ov_backend->get_model_input_names());
    model_info.set_layer_names("output", _ov_backend->get_model_output_names());

    if (!_post_processing_elem->process(pp_frame))
        _log->warn("post-processing completed with error");

    // FIXME: erase model metadata
    auto it = std::find_if(pp_frame->metadata().begin(), pp_frame->metadata().end(),
                           [](auto &item) { return item->name() == ModelInfoMetadata::name; });
    if (it != pp_frame->metadata().end())
        pp_frame->metadata().erase(it);
}

void FrameInference::fake_start_inference_internal(FramePtr frame) {
    {
        std::lock_guard<std::mutex> stub_lock(_stub.infer_list_mutex);
        _stub.infer_list.push_back(std::move(frame));
    }
    _stub.ov_signal.notify_one();
}

void FrameInference::fake_ov_worker() {
    _log->debug("Fake OV worker: started");

    auto flush = [this]() {
        std::list<FramePtr> l;
        {
            std::lock_guard<std::mutex> lock(_stub.infer_list_mutex);
            std::swap(_stub.infer_list, l);
        }

        _log->debug("Fake OV worker: flushing frames count {}", l.size());

        while (!l.empty()) {
            FramePtr f;
            l.back().swap(f);
            l.pop_back();
            on_inference_complete(std::move(f), {});
        }
    };

    while (_stub.thread_running) {
        if (_stub.flush) {
            _log->debug("Fake OV worker: flush requested");
            flush();
            _stub.flush = false;
        }

        FramePtr f;
        {
            std::unique_lock<std::mutex> lock(_stub.infer_list_mutex);
            _stub.ov_signal.wait(lock, [this] { return _stub.flush || !_stub.infer_list.empty(); });

            if (_stub.infer_list.empty())
                continue;
            _stub.infer_list.back().swap(f);
            _stub.infer_list.pop_back();
        }

        assert(f);
        on_inference_complete(std::move(f), {});
    }

    _log->debug("Fake OV worker: rundown");

    flush();

    _log->debug("Fake OV worker: exited");
}