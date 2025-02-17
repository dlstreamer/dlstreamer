/*******************************************************************************
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer_logger.h"

#include "openvino.hpp"

#include "dlstreamer/openvino/context.h"
#include "dlstreamer/openvino/tensor.h"
#include "dlstreamer/openvino/utils.h"
#include "infer_requests_queue.hpp"

#include <queue>

#include "dlstreamer/element.h"

namespace dlstreamer {

namespace param {
static constexpr auto model = "model";   // path to model file
static constexpr auto device = "device"; // string
static constexpr auto config = "config"; // string, comma separated list of KEY=VALUE parameters
static constexpr auto batch_size = "batch-size";
static constexpr auto nireq = "nireq";
}; // namespace param

class OpenVinoInference {
    friend class OpenVinoBackend;

  public:
    using InferenceCompleteCallback = OpenVinoBackend::InferenceCompleteCallback;

    OpenVinoInference(DictionaryCPtr params, FrameInfo &input_info)
        : _params(params), _input_info(input_info),
          _logger(log::get_or_nullsink(params->get(param::logger_name, std::string()))), _requests_processing(0U) {
        auto task = itt::Task("openvino:OpenVinoInference");
        _device = _params->get<std::string>(param::device);
        read_ir_model();
        if (is_preprocessing_required())
            configure_model_preprocessing();
        load_network();
        if (!_openvino_context)
            _openvino_context = std::make_shared<OpenVINOContext>(_compiled_model);
    }

    void infer(FrameVector frames, InferenceCompleteCallback complete_cb) { // TODO What shoud return
        auto task = itt::Task("openvino:OpenVinoInference:infer");
        if (frames.empty())
            throw std::invalid_argument("tensors cannot be empty");
        if (!complete_cb)
            throw std::invalid_argument("complete_cb cannot be empty");

        std::unique_lock<std::mutex> lk(_requests_mutex);
        ++_requests_processing;

        auto tensors = map_frames_to_tensors(frames);
        size_t idx = 0;
        for (auto frame_tensors : tensors) { // TODO: need to understand which frame maps to which tensors
            auto batch_request = get_free_infer_request();
            set_input(frame_tensors, batch_request->infer_request);
            // Not accurate
            batch_request->frame = frames[idx];
            idx++;

            batch_request->complete_cb = complete_cb;
            batch_request->infer_request.start_async();
        }
    }

    FrameInfo get_model_input() const {
        return _model_input_info;
    }

    FrameInfo get_model_output() const {
        return _model_output_info;
    }

    void flush() {
        auto task = itt::Task("openvino:OpenVinoInference:flush");
        std::unique_lock<std::mutex> requests_lk(_requests_mutex);
        std::unique_lock<std::mutex> flush_lk(_flush_mutex);
        _request_processed.wait_for(flush_lk, std::chrono::seconds(1), [this] { return _requests_processing == 0; });
    }

  private:
    ov::Core _core; // TODO Ov reccomends to use single tone for core
    DictionaryCPtr _params;
    std::string _device;
    std::shared_ptr<ov::Model> _model;
    FrameInfo _input_info;
    FrameInfo _model_input_info;
    FrameInfo _model_output_info;
    std::vector<std::string> _model_input_names;
    std::vector<std::string> _model_output_names;
    ov::CompiledModel _compiled_model;
    std::shared_ptr<OpenVINOContext> _openvino_context;
    struct BatchRequest {
        ov::InferRequest infer_request;
        InferenceCompleteCallback complete_cb;
        FramePtr frame;
        // std::vector<InferenceBackend::Allocator::AllocContext *> alloc_context; // TODO Openvino context for shared
        // mem
    };
    SafeQueue<std::shared_ptr<BatchRequest>> _free_requests; // TODO Should it be safe queue?
    int _nireq;

    std::shared_ptr<spdlog::logger> _logger;

    std::mutex _requests_mutex;
    std::atomic<unsigned int> _requests_processing;
    std::condition_variable _request_processed;
    std::mutex _flush_mutex;

    std::shared_ptr<BatchRequest> get_free_infer_request() {
        auto task = itt::Task("openvino:OpenVinoInference:get_free_infer_request");
        return _free_requests.pop();
    }

    bool is_preprocessing_required() const {
        return true; // TODO Actual logic here
    }

    void configure_model_preprocessing() {
        auto task = itt::Task("openvino:OpenVinoInference:configure_model_preprocessing");
        ov::preprocess::PrePostProcessor ppp(_model);

        auto &ppp_input = ppp.input();

        const auto color_fmt_pair = image_format_to_ov(static_cast<ImageFormat>(_input_info.format));
        ppp_input.tensor()
            .set_element_type(ov::element::u8)
            .set_color_format(color_fmt_pair.first, color_fmt_pair.second);

        bool apply_resize = false;
        if (_input_info.memory_type == MemoryType::VAAPI) {
            // Configure model's pre-processing for VAAPI NV12 surface input
            ppp_input.tensor().set_memory_type(ov::intel_gpu::memory_type::surface);
        } else {
            // System memory input
            assert(_input_info.memory_type == MemoryType::CPU);
            apply_resize = true;

            // if (_params->get<bool>(param::dynamic_input, false)) { // # TODO Do we need dynamic input?
            if (0) {
                ppp_input.tensor().set_spatial_dynamic_shape();
            } else {
                const auto &in_shape = _input_info.tensors.front().shape;
                ImageLayout in_layout(in_shape);
                const size_t height = in_shape.at(in_layout.h_position());
                const size_t width = in_shape.at(in_layout.w_position());

                ppp_input.tensor().set_spatial_static_shape(height, width);
            }
        }

        ppp_input.preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
        if (apply_resize)
            ppp_input.preprocess().resize(ov::preprocess::ResizeAlgorithm::RESIZE_LINEAR);
        ppp_input.tensor().set_layout("NHWC");
        ppp_input.model().set_layout("NCHW");

        _model = ppp.build();
    }

    static std::pair<ov::preprocess::ColorFormat, std::vector<std::string>>
    image_format_to_ov(ImageFormat image_format) {
        auto task = itt::Task("openvino:OpenVinoInference:image_format_to_ov");
        switch (image_format) {
        case ImageFormat::NV12:
            return {ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"}};

        case ImageFormat::BGRX:
            return {ov::preprocess::ColorFormat::BGRX, {}};

        default:
            auto msg = fmt::format("Unsupported image color format: {}", image_format_to_string(image_format));
            throw std::runtime_error(msg);
        }
    }

    void allocate_infer_requests() {
        auto task = itt::Task("openvino:OpenVinoInference:allocate_infer_requests");
        for (int i = 0; i < _nireq; i++) {
            std::shared_ptr<BatchRequest> batch_request = std::make_shared<BatchRequest>();
            batch_request->infer_request = _compiled_model.create_infer_request();
            set_completion_callback(batch_request);
            // if (allocator) {
            //     SetBlobsToInferenceRequest(layers, batch_request, allocator);
            // }
            _free_requests.push(batch_request);
        }
    }

    void free_request(std::shared_ptr<BatchRequest> batch_request) {
        auto task = itt::Task("openvino:OpenVinoInference:free_request");
        _free_requests.push(batch_request);
        _requests_processing -= 1;
        _request_processed.notify_all();
    }

    std::vector<TensorVector> map_frames_to_tensors(FrameVector &frames) {
        auto task = itt::Task("openvino:OpenVinoInference:map_frames_to_tensors");
        std::vector<TensorVector> frames_tensors;
        for (auto frame : frames) {
            frames_tensors.push_back(map_image_frame(frame));
        }
        return frames_tensors;
    }

    TensorVector map_image_frame(FramePtr src) {
        auto task = itt::Task("openvino:OpenVinoInference:map_image_frame");
        TensorVector tensors;
        const auto img_format = static_cast<ImageFormat>(src->format());
        // FIXME: Add support for I420
        if (img_format == ImageFormat::I420) {
            throw std::runtime_error("Unsupported color format " + image_format_to_string(img_format));
        }

        assert(img_format == ImageFormat::NV12 || img_format == ImageFormat::BGRX);

        if (img_format == ImageFormat::NV12 && src->num_tensors() != 2)
            throw std::runtime_error("For NV12 image two planes (tensors) are expected");

        std::vector<size_t> shape(4), strides(4);
        for (const auto &src_tensor : src) {
            // Convert layout if necessary. CHW/HWC -> NCHW/NHWC
            if (src_tensor->info().shape.size() == 3) {
                shape.resize(1);
                shape[0] = 1;
                const auto src_shape = src_tensor->info().shape;
                shape.insert(shape.end(), src_shape.begin(), src_shape.end());

                strides.resize(1);
                strides.insert(strides.end(), src_tensor->info().stride.begin(), src_tensor->info().stride.end());
                // FIXME: Actually not correct in case of cropped tensor since we don't know correct dims
                strides[0] = strides[1] * shape[1];
            } else {
                shape = src_tensor->info().shape;
                strides = src_tensor->info().stride;
            }

            auto ov_tensor = ov::Tensor(ov::element::u8, shape, src_tensor->data(), strides);
            tensors.push_back(std::make_shared<OpenVINOTensor>(ov_tensor, _openvino_context));
        }

        return tensors;
    }

    void set_input(TensorVector &tensors, ov::InferRequest &infer_request) {
        auto task = itt::Task("openvino:OpenVinoInference:set_input");
        for (size_t i = 0; i < tensors.size(); i++) {
            auto task_single = itt::Task("openvino:OpenVinoInference:set_input:single_tensor_set_input");
            auto ov_tensors = std::dynamic_pointer_cast<OpenVINOTensorBatch>(tensors[i]);
            if (ov_tensors) {
                infer_request.set_input_tensors(i, ov_tensors->tensors());
            } else {
                auto ov_tensor = ptr_cast<OpenVINOTensor>(tensors[i]);
                infer_request.set_input_tensor(i, *ov_tensor);
            }
        }
    }

    void read_ir_model() {
        // read model
        auto task = itt::Task("openvino:OpenVinoInference:read_ir_model");
        auto path = _params->get<std::string>(param::model);
        _logger->debug("reading model file '{}'", path);
        _model = _core.read_model(path);
        // set batch size
        int batch_size = _params->get<int>(param::batch_size);
        if (batch_size > 1)
            ov::set_batch(_model, batch_size);
        // get input info
        _model_input_info = FrameInfo(MediaType::Tensors);
        for (auto node : _model->get_parameters()) {
            auto dtype = data_type_from_openvino(node->get_element_type());
            auto shape = node->is_dynamic() ? node->get_input_partial_shape(0).get_min_shape() : node->get_shape();
            _model_input_info.tensors.push_back(TensorInfo(shape, dtype));
            _model_input_names.push_back(node->get_friendly_name());
        }
        // get output info
        _model_output_info = FrameInfo(MediaType::Tensors);
        for (auto node : _model->get_results()) {
            auto dtype = data_type_from_openvino(node->get_element_type());
            auto shape = node->is_dynamic() ? node->get_output_partial_shape(0).get_min_shape() : node->get_shape();
            _model_output_info.tensors.push_back(TensorInfo(shape, dtype));
        }
        for (auto &output : _model->outputs()) {
            _model_output_names.push_back(output.get_any_name());
        }
    }
    void print_ov_map(const ov::AnyMap &map) {
        for (auto &item : map) {
            std::cout << "  " << item.first << ": " << item.second.as<std::string>() << std::endl;
        }
    }
    // Loads network to the device
    void load_network() {
        auto task = itt::Task("openvino:OpenVinoInference:load_network");
        if (!_compiled_model) {
            std::string config = _params->get<std::string>(param::config);
            auto ov_params = string_to_openvino_map(config);
            adjust_ie_config(ov_params); // TODO Do we need it?

            // Adjusting nireq
            _nireq = _params->get<int>(param::nireq);
            if (_nireq != 0) {
                ov_params[ov::hint::num_requests.name()] = _nireq;
            }

            std::cout << "Params for compile_model:\n";
            print_ov_map(ov_params);
            _compiled_model = _core.compile_model(_model, _device, ov_params);
            if (_nireq == 0) {
                // TODO add Try catch
                _nireq = _compiled_model.get_property(ov::optimal_number_of_infer_requests);
            }

            allocate_infer_requests();
            // if (_openvino_context) {
            //     _compiled_model = _core.compile_model(_model, *_openvino_context, ov_params);
            // } else {
            //     _compiled_model = _core.compile_model(_model, _device, ov_params);
            // }
            std::cout << "Network loaded to device" << std::endl;
            auto supported_properties = _compiled_model.get_property(ov::supported_properties);
            for (const auto &cfg : supported_properties) {
                if (cfg == ov::supported_properties)
                    continue;
                auto prop = _compiled_model.get_property(cfg);
                std::cout << " " << cfg << ": " << prop.as<std::string>() << std::endl;
            }
        }
    }
    // TODO This just copypast from openvino_inference.cpp
    std::string get_device_type() const {
        return _device.substr(0, _device.find_first_of(".("));
    }

    // TODO This just copypast from openvino_inference.cpp
    static ov::AnyMap string_to_openvino_map(const std::string &s, char rec_delim = ',', char kv_delim = '=') {
        std::string key, val;
        std::istringstream iss(s);
        ov::AnyMap m;

        while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
            m.emplace(std::move(key), std::move(val));
        }

        return m;
    }

    // TODO This just copypast from openvino_inference.cpp
    void adjust_ie_config(ov::AnyMap &ie_config) {
        auto task = itt::Task("openvino:OpenVinoInference:adjust_ie_config");
        const std::string num_streams_key = get_device_type() + "_THROUGHPUT_STREAMS";
        if (ie_config.count(ov::num_streams.name()) || ie_config.count(num_streams_key))
            return;
        if (ie_config.count(ov::hint::performance_mode.name()) || ie_config.count(ov::hint::num_requests.name()))
            return;

        auto supported_properties = _core.get_property(_device, ov::supported_properties);
        auto supported = [&](const std::string &key) {
            return std::find(std::begin(supported_properties), std::end(supported_properties), key) !=
                   std::end(supported_properties);
        };

        if (supported(ov::hint::performance_mode.name()))
            ie_config[ov::hint::performance_mode.name()] = ov::hint::PerformanceMode::THROUGHPUT;
        else if (supported(num_streams_key))
            ie_config[num_streams_key] = std::string(get_device_type() + "_THROUGHPUT_AUTO");
        else if (supported(ov::num_streams.name()))
            ie_config[ov::num_streams.name()] = ov::streams::AUTO;
    }

    void process_results(std::shared_ptr<BatchRequest> batch_request) {
        auto task = itt::Task("openvino:OpenVinoInference:process_results");
        auto num_tensors = batch_request->infer_request.get_compiled_model().outputs().size();

        // FIXME optimize!
        TensorVector tensors_vec(num_tensors);
        for (size_t i = 0; i < num_tensors; i++) {
            auto task = itt::Task("openvino:OpenVinoInference:process_results:single_tensor");
            auto tensor =
                std::make_shared<OpenVINOTensor>(batch_request->infer_request.get_output_tensor(i), nullptr,
                                                 nullptr); // TODO We do not need wait function here as we
                                                           // called it from callback. TODO Do we need context?
            tensors_vec[i] = std::move(tensor);
        }

        batch_request->complete_cb(std::move(batch_request->frame), std::move(tensors_vec));
    }

    void set_completion_callback(std::shared_ptr<BatchRequest> batch_request) {
        auto task = itt::Task("openvino:OpenVinoInference:set_completion_callback");
        assert(batch_request && "Batch request is null");

        auto completion_callback = [=, this](std::exception_ptr ex) {
            _logger->trace("inference completed");
            if (ex) {
                // TODO How do we handle exeptions
                // Print exception itself
                _logger->error("exception occured during inference");
            } else {
                this->process_results(batch_request);
            }
            free_request(batch_request);
        };
        batch_request->infer_request.set_callback(completion_callback);
    }
};

OpenVinoBackend::OpenVinoBackend(DictionaryCPtr params, FrameInfo &input_info)
    : _impl(std::make_unique<OpenVinoInference>(params, input_info)) {
}

OpenVinoBackend::~OpenVinoBackend() = default;

void OpenVinoBackend::infer_async(FrameVector frames, InferenceCompleteCallback complete_cb) {
    _impl->infer(std::move(frames), std::move(complete_cb));
}

const std::string &OpenVinoBackend::get_model_name() const {
    return _impl->_model->get_friendly_name();
}

FrameInfo OpenVinoBackend::get_model_input() const {
    return _impl->get_model_input();
}

FrameInfo OpenVinoBackend::get_model_output() const {
    return _impl->get_model_output();
}

const std::vector<std::string> &OpenVinoBackend::get_model_input_names() const {
    return _impl->_model_input_names;
}

const std::vector<std::string> &OpenVinoBackend::get_model_output_names() const {
    return _impl->_model_output_names;
}

void OpenVinoBackend::flush() {
    _impl->flush();
}

} // namespace dlstreamer