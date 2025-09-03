/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/openvino/elements/openvino_inference.h"

#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/openvino/context.h"
#include "dlstreamer/vaapi/context.h"

#include <logger.h>
#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/properties.hpp>

namespace dlstreamer {

namespace param {
static constexpr auto model = "model";   // path to model file
static constexpr auto device = "device"; // string
static constexpr auto config = "config"; // string, comma separated list of KEY=VALUE parameters
static constexpr auto batch_size = "batch-size";
static constexpr auto buffer_pool_size = "buffer-pool-size";
}; // namespace param

static ParamDescVector params_desc = {
    {
        param::model,
        "Path to model file in OpenVINO™ toolkit or ONNX format",
        "",
    },
    {
        param::device,
        "Target device for inference. Please see OpenVINO™ toolkit documentation for list of supported devices.",
        "CPU",
    },
    {param::config, "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", ""},
    {param::batch_size, "Batch size", 1, 0, std::numeric_limits<int>::max()},
    {param::buffer_pool_size, "Output buffer pool size (functionally same as OpenVINO™ toolkit nireq parameter)", 16, 0,
     std::numeric_limits<int>::max()},
};

class OpenVinoTensorInference : public BaseTransform {
  public:
    OpenVinoTensorInference(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransform(app_context), _params(params) {
        _device = _params->get<std::string>(param::device);
        _buffer_pool_size = _params->get<int>(param::buffer_pool_size, 16);
        read_ir_model();
    }

    FrameInfoVector get_input_info() override {
        auto infos = info_variations(_model_input_info, {MemoryType::OpenCL, MemoryType::CPU},
                                     {DataType::UInt8, DataType::Float32});
        return infos;
    }

    FrameInfoVector get_output_info() override {
        _model_output_info.memory_type = MemoryType::OpenVINO;
        return {_model_output_info};
    }

    void read_ir_model() {
        // read model
        auto path = _params->get<std::string>(param::model);
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

    bool init_once() override {
        if (is_preprocessing_required())
            configure_model_preprocessing();

        load_network();

        if (!_input_mapper) {
            ContextPtr interm_context = std::make_shared<BaseContext>(_input_info.memory_type);
            if (!_openvino_context)
                _openvino_context = std::make_shared<OpenVINOContext>(_compiled_model);
            _input_mapper = create_mapper({_app_context, interm_context, _openvino_context});
        }

        // print all properties
        try {
            auto supported_properties = _compiled_model.get_property(ov::supported_properties);
            for (const auto &cfg : supported_properties) {
                if (cfg == ov::supported_properties)
                    continue;
                auto prop = _compiled_model.get_property(cfg);
                GVA_INFO("OpenVINO™ toolkit config: %s \t= %s", cfg.c_str(), prop.as<std::string>().c_str());
            }
        } catch (const ov::Exception &) {
        }

        return true;
    }

    ContextPtr get_context(MemoryType memory_type) noexcept override {
        try {
            if (memory_type == MemoryType::OpenCL) {
                return create_remote_context();
            }
        } catch (const std::out_of_range &e) {
            GVA_ERROR("Out of range error in get_context: %s", e.what());
        } catch (...) {
            GVA_ERROR("Unknown exception occurred in get_context");
        }
        return nullptr;
    }

    virtual std::function<FramePtr()> get_output_allocator() override {
        return [this]() {
            auto infer_request = _compiled_model.create_infer_request();
            return std::make_shared<OpenVINOFrame>(infer_request, _openvino_context);
        };
    }

    FramePtr process(FramePtr src) override {
        DLS_CHECK(init());
        // ITT_TASK(__FUNCTION__);
        FramePtr dst = create_output();
        auto src_openvino = _input_mapper->map(src, AccessMode::Read);
        auto dst_openvino = ptr_cast<OpenVINOFrame>(dst);

        // FIXME: Since we are reusing inference requests we have to make sure that the inference is actually completed.
        // This is usually done when buffer is mapped somewhere in downstream.
        // However, we should properly handle the situation if buffer was not mapped in downstream.
        // For example, fakesink after inference.
        dst_openvino->wait();

        dst_openvino->set_input({src_openvino.begin(), src_openvino.end()});

        // capture input tensors until infer_request.wait() completed
        dst_openvino->set_parent(src_openvino);

        dst_openvino->start();

        ModelInfoMetadata model_info(dst->metadata().add(ModelInfoMetadata::name));
        model_info.set_model_name(_model->get_friendly_name());
        model_info.set_info("input", _model_input_info);
        model_info.set_info("output", _model_output_info);
        model_info.set_layer_names("input", _model_input_names);
        model_info.set_layer_names("output", _model_output_names);

        return dst;
    }

  protected:
    ov::Core _core;
    std::string _device;
    std::shared_ptr<ov::Model> _model;
    ov::CompiledModel _compiled_model;

    FrameInfo _model_input_info;
    FrameInfo _model_output_info;
    std::vector<std::string> _model_input_names;
    std::vector<std::string> _model_output_names;
    DictionaryCPtr _params;
    MemoryMapperPtr _input_mapper;
    OpenVINOContextPtr _openvino_context;

    bool is_device_gpu() const {
        return _device.find("GPU") != std::string::npos;
    }

    std::string get_device_type() const {
        return _device.substr(0, _device.find_first_of(".("));
    }

    ContextPtr create_remote_context() {
        if (is_device_gpu() && !_openvino_context) {
            try {
                VAAPIContextPtr vaapi_ctx = VAAPIContext::create(_app_context);
                _openvino_context = std::make_shared<OpenVINOContext>(_core, _device, vaapi_ctx);
            } catch (std::exception &e) {
                printf("Exception creating OpenVINO™ toolkit remote context: %s\n", e.what());
                load_network();
                _openvino_context = std::make_shared<OpenVINOContext>(_compiled_model);
            }
        }
        return _openvino_context;
    }

    virtual bool is_preprocessing_required() const {
        return _input_info.tensors != _model_input_info.tensors || _input_info.media_type != MediaType::Tensors;
    }

    // Setups model preprocessing
    virtual void configure_model_preprocessing() {
        if (_input_info.media_type != MediaType::Tensors)
            throw std::runtime_error("Tensor input is expected");

        ov::preprocess::PrePostProcessor ppp(_model);
        auto &ppp_input = ppp.input();

        if (_input_info.tensors.size() != 1 || _model_input_info.tensors.size() != 1)
            throw std::runtime_error("Can't enable pre-processing on model with multiple tensors input");
        auto &model_info = _model_input_info.tensors.front();
        auto &requested_info = _input_info.tensors.front();

        if (requested_info.dtype != model_info.dtype)
            ppp_input.tensor().set_element_type(data_type_to_openvino(requested_info.dtype));
        if (requested_info.shape != model_info.shape)
            ppp_input.tensor().set_shape({requested_info.shape});

        // ppp_input.model().set_layout("NHWC");
        // ppp_input.preprocess().convert_color(ov::preprocess::ColorFormat::BGRX);
        // ppp_input.tensor().set_memory_type(ov::intel_gpu::memory_type::buffer);

        // Add pre-processing to model
        _model = ppp.build();
    }

    // Loads network to the device
    void load_network() {
        if (!_compiled_model) {
            std::string config = _params->get<std::string>(param::config);
            auto ov_params = string_to_openvino_map(config);
            adjust_ie_config(ov_params);
            if (_openvino_context) {
                _compiled_model = _core.compile_model(_model, *_openvino_context, ov_params);
            } else {
                _compiled_model = _core.compile_model(_model, _device, ov_params);
            }
        }
    }

    FrameInfoVector info_variations(const FrameInfo &info, std::vector<MemoryType> memory_types,
                                    std::vector<DataType> data_types) {
        FrameInfoVector infos;
        for (MemoryType mtype : memory_types) {
            for (DataType dtype : data_types) {
                FrameInfo info2 = info;
                info2.memory_type = mtype;
                for (auto &tensor : info2.tensors) {
                    tensor.dtype = dtype;
                    tensor.stride = contiguous_stride(tensor.shape, dtype);
                }
                infos.push_back(info2);
            }
        }
        return infos;
    }

    static ov::AnyMap string_to_openvino_map(const std::string &s, char rec_delim = ',', char kv_delim = '=') {
        std::string key, val;
        std::istringstream iss(s);
        ov::AnyMap m;

        while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
            m.emplace(std::move(key), std::move(val));
        }

        return m;
    }
    void adjust_ie_config(ov::AnyMap &ie_config) {
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
};

class OpenVinoVideoInference : public OpenVinoTensorInference {
  public:
    using OpenVinoTensorInference::OpenVinoTensorInference;

    FrameInfoVector get_input_info() override {
        assert(_model_input_info.tensors.size() == 1);
        FrameInfo frame_info(ImageFormat::NV12, MemoryType::VAAPI, {_model_input_info.tensors.front()});

        return {frame_info};
    }

    bool init_once() override {
        if (_input_info.media_type != MediaType::Image)
            throw std::runtime_error("Image input is expected");
        if (_input_info.memory_type != MemoryType::VAAPI ||
            static_cast<ImageFormat>(_input_info.format) != ImageFormat::NV12) {
            throw std::runtime_error("Image input is supported only for NV12 image format and VASurface memory");
        }
        if (!is_device_gpu())
            throw std::runtime_error("VASurface as input supported only for inference on GPU");

        create_remote_context();

        // Chain to base implementation
        return OpenVinoTensorInference::init_once();
    }

  protected:
    bool is_preprocessing_required() const override {
        // The pre-processing configuration is required for NV12 VASurface input
        return true;
    }

    // Setups model preprocessing
    void configure_model_preprocessing() override {
        ov::preprocess::PrePostProcessor ppp(_model);
        auto &ppp_input = ppp.input();

        // Configure model's pre-processing for VAAPI NV12 surface input
        ppp_input.tensor()
            .set_element_type(ov::element::u8)
            .set_color_format(ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"})
            .set_memory_type(ov::intel_gpu::memory_type::surface);
        ppp_input.preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
        ppp_input.tensor().set_layout("NHWC");
        ppp_input.model().set_layout("NCHW");

        _model = ppp.build();
    }
};

extern "C" {
ElementDesc openvino_tensor_inference = {
    .name = "openvino_tensor_inference",
    .description = "Inference on OpenVINO™ toolkit backend",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info =
        MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::OpenCL}, {MediaType::Tensors, MemoryType::CPU}}),
    .output_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::OpenVINO}}),
    .create = create_element<OpenVinoTensorInference>,
    .flags = ELEMENT_FLAG_SHARABLE};
}

extern "C" {
ElementDesc openvino_video_inference = {.name = "openvino_video_inference",
                                        .description = "Inference on OpenVINO™ toolkit backend",
                                        .author = "Intel Corporation",
                                        .params = &params_desc,
                                        .input_info = MAKE_FRAME_INFO_VECTOR({{ImageFormat::NV12, MemoryType::VAAPI}}),
                                        .output_info =
                                            MAKE_FRAME_INFO_VECTOR({{MediaType::Tensors, MemoryType::OpenVINO}}),
                                        .create = create_element<OpenVinoVideoInference>,
                                        .flags = ELEMENT_FLAG_SHARABLE};
}

} // namespace dlstreamer
