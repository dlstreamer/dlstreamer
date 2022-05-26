/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_openvino.h"
#include "dlstreamer/buffer_mappers/cpu_to_openvino.h"
#include "dlstreamer/buffer_mappers/mapper_chain.h"
#include "dlstreamer/buffer_mappers/opencl_to_openvino.h"
#include "dlstreamer/buffer_mappers/openvino_to_cpu.h"
#include "dlstreamer/openvino/buffer.h"
#include "dlstreamer/openvino/utils.h"
#include "dlstreamer/vaapi/context.h"

#include <gpu/gpu_params.hpp>
#include <ie_compound_blob.h>
#include <inference_engine.hpp>
#include <thread>

namespace dlstreamer {

namespace param {
static constexpr auto model = "model";
static constexpr auto device = "device";
static constexpr auto ie_config = "ie-config";
static constexpr auto batch_size = "batch-size";
}; // namespace param

static ParamDescVector params = {
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
    {param::ie_config, "Comma separated list of KEY=VALUE parameters for Inference Engine configuration", ""},
    {param::batch_size, "Batch size", 1, 0, INT_MAX}};

class InferenceOpenVINO : public TransformWithAlloc {
  public:
    InferenceOpenVINO(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformWithAlloc(transform_ctrl, std::move(params)) {
        read_ir_model();

        // try_init_ocl_context();
        // try_init_ocl_context can't be called here due to lack of VAAPIContext.
        // The VA Display is initialized in start() callback of VAAPI elements.
        // The start() calls happens from downstream to upstream.
        // The Transform constructor (this one) is called from start() callback
        // and since our elements are downstream with respect to the VAAPI elements,
        // we cannot get the VADisplay here – it's not yet created.
    }

    BufferInfoVector get_input_info(const BufferInfo & /*output_info*/) override {
        return info_from_ie(_inputs, {DataType::U8, DataType::FP32});
    }
    BufferInfoVector get_output_info(const BufferInfo & /*input_info*/) override {
        return info_from_ie(_outputs, {DataType(0)});
    }

    void read_ir_model() {
        std::string model_xml_file = _params->get<std::string>(param::model);
        std::string default_bin_file = model_xml_file.substr(0, model_xml_file.size() - 4) + ".bin";
        std::string model_bin_file = default_bin_file;

        _cnn_network = _ie.ReadNetwork(model_xml_file, model_bin_file);
        int batch_size = _params->get<int>(param::batch_size);
        _cnn_network.setBatchSize(batch_size);
        _inputs = _cnn_network.getInputsInfo();
        _outputs = _cnn_network.getOutputsInfo();
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        if (!set_ie_info(input_info, _inputs))
            throw std::runtime_error("Couldn't set input to IE based on BufferInfo");
        if (!set_ie_info(output_info, _outputs))
            throw std::runtime_error("Couldn't set output to IE based on BufferInfo");

        if (!_executable_network)
            load_network();
        if (!_input_mapper) {
            std::vector<BufferMapperPtr> chain;
            if (input_info.buffer_type == BufferType::OPENCL_BUFFER) {
                try_init_ocl_context();
                chain = {_transform_ctrl->create_input_mapper(BufferType::OPENCL_BUFFER, _opencl_context),
                         std::make_shared<BufferMapperOpenCLToOpenVINO>(_executable_network.GetContext())};
            } else {
                chain = {_transform_ctrl->create_input_mapper(BufferType::CPU),
                         std::make_shared<BufferMapperCPUToOpenVINO>()};
            }
            _input_mapper = std::make_shared<BufferMapperChain>(chain);
        }
    }

    ContextPtr get_context(const std::string &name) override {
        if (name == OpenCLContext::context_name) {
            if (!_opencl_context)
                try_init_ocl_context();
            return _opencl_context;
        }
        return nullptr;
    }

    virtual std::function<BufferPtr()> get_output_allocator() override {
        return [this]() {
            auto infer_request = _executable_network.CreateInferRequestPtr();
            OpenVINOBlobsBuffer::Blobs blobs;
            for (const auto &info : _outputs) {
                blobs.push_back(infer_request->GetBlob(info.first));
            }
            return std::make_shared<OpenVINOBlobsBuffer>(blobs, infer_request);
        };
    }

    virtual BufferMapperPtr get_output_mapper() override {
        return std::make_shared<BufferMapperOpenVINOToCPU>();
    }

    bool process(BufferPtr src, BufferPtr dst) override {
        // ITT_TASK(__FUNCTION__);

        auto src_openvino = _input_mapper->map<OpenVINOBlobsBuffer>(src, AccessMode::READ);
        auto dst_openvino = std::dynamic_pointer_cast<OpenVINOBlobsBuffer>(dst);
        auto infer_request = dst_openvino->infer_request();

        // FIXME: Since we are reusing inference requests we have to make sure that the inference is actually completed.
        // This is usually done when buffer is mapped somewhere in downstream.
        // However, we should properly handle the situation if buffer was not mapped in downstream.
        // For example, fakesink after inference.
        infer_request->Wait();

        size_t i = 0;
        for (const auto &info : _inputs) {
            infer_request->SetBlob(info.first, src_openvino->blob(i++));
        }
        dst_openvino->capture_input(src_openvino);
        infer_request->StartAsync();

        return true;
    }

  protected:
    IE::Core _ie;
    IE::CNNNetwork _cnn_network;
    IE::InputsDataMap _inputs;
    IE::OutputsDataMap _outputs;
    IE::ExecutableNetwork _executable_network;
    IE::RemoteContext::Ptr _ie_remote_context;

    BufferMapperPtr _input_mapper;
    OpenCLContextPtr _opencl_context;

    void init_remote_context() {
        if (_ie_remote_context)
            return;

        std::string device = _params->get<std::string>(param::device);
        if (device.find("GPU") != std::string::npos)
            create_gpu_remote_context(device);
    }

    void create_gpu_remote_context(const std::string &device) {
        auto vaapi_ctx = _transform_ctrl->get_context<VAAPIContext>();
        if (!vaapi_ctx) {
            // As fallback we can enum OpenVINO™ toolkit devices and create remote context based on them
            throw std::runtime_error("Can't query VAAPI context");
        }

        using namespace InferenceEngine;
        ParamMap contextParams = {{GPU_PARAM_KEY(CONTEXT_TYPE), GPU_PARAM_VALUE(VA_SHARED)},
                                  {GPU_PARAM_KEY(VA_DEVICE), static_cast<gpu_handle_param>(vaapi_ctx->va_display())}};

#if 0 // TODO remove try/catch workaround after migration to OpenVINO™ toolkit 2022.1
        _ie_remote_context = _ie.CreateContext(device, contextParams);
#else
        try {
            _ie_remote_context = _ie.CreateContext(device, contextParams);
        } catch (std::exception &e) {
            printf("Exception creating OpenVINO™ toolkit remote context: %s", e.what());
            load_network(false);
            _ie_remote_context = _executable_network.GetContext();
        }
#endif
    }

    // Tries to initialize OpenCL context from OV remote context
    void try_init_ocl_context() {
        try {
            init_remote_context();

            // Try to get cl_context from OV remote context
            if (_ie_remote_context) {
                auto ctx_params = _ie_remote_context->getParams();
                auto ctx_param = ctx_params.find(IE::GPUContextParams::PARAM_OCL_CONTEXT);
                if (ctx_param != ctx_params.end()) {
                    auto cl_ctx = static_cast<cl_context>(ctx_param->second.as<IE::gpu_handle_param>());
                    // Initialize our OpenCLContext with cl_context from OV
                    // Make sure that IE context outlives the our OpenCL context
                    auto deleter = [ie_ctx = _ie_remote_context](OpenCLContext *ctx) { delete ctx; };
                    _opencl_context = OpenCLContextPtr(new OpenCLContext(cl_ctx), deleter);
                }
            }
        } catch (...) {
        }
    }

    // Loads network to the device
    void load_network(bool create_context = true) {
        if (create_context)
            init_remote_context();

        std::string ie_config = _params->get<std::string>(param::ie_config);

        if (_ie_remote_context) {
            _executable_network = _ie.LoadNetwork(_cnn_network, _ie_remote_context, string_to_map(ie_config));
        } else {
            std::string device = _params->get<std::string>(param::device);
            _executable_network = _ie.LoadNetwork(_cnn_network, device, string_to_map(ie_config));
        }
    }

    template <class T>
    BufferInfoVector info_from_ie(const T &ie_map, std::vector<DataType> data_types) {
        BufferInfoVector infos;
        for (DataType type : data_types) {
            BufferInfo info(MediaType::TENSORS);
            for (const auto &ie_info : ie_map) {
                IE::TensorDesc desc = ie_info.second->getTensorDesc();
                PlaneInfo plane = tensor_desc_to_plane_info(desc, ie_info.first);
                if (type != DataType(0))
                    plane.type = type;
                info.planes.push_back(plane);
            }
            infos.push_back(info);
        }
        return infos;
    }

    template <class T>
    bool set_ie_info(const BufferInfo &info, T &ie_map) {
        if (info.planes.size()) {
            size_t i = 0;
            for (const auto &ie_info : ie_map) {
                if (info.planes[i].shape != ie_info.second->getTensorDesc().getDims())
                    return false;
                ie_info.second->setPrecision(data_type_to_openvino(info.planes[i].type));
                i++;
            }
        }
        return true;
    }

    static std::map<std::string, std::string> string_to_map(const std::string &s, char rec_delim = ',',
                                                            char kv_delim = '=') {
        std::string key, val;
        std::istringstream iss(s);
        std::map<std::string, std::string> m;

        while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
            m.emplace(std::move(key), std::move(val));
        }

        return m;
    }
};

TransformDesc TensorInferenceOpenVINODesc = {
    .name = "tensor_inference_openvino",
    .description = "Inference on OpenVINO™ toolkit backend",
    .author = "Intel Corporation",
    .params = &params,
    .input_info = {{MediaType::TENSORS, BufferType::CPU}, {MediaType::TENSORS, BufferType::OPENCL_BUFFER}},
    .output_info = {{MediaType::TENSORS, BufferType::OPENVINO}},
    .create = TransformBase::create<InferenceOpenVINO>,
    .flags = TRANSFORM_FLAG_OUTPUT_ALLOCATOR | TRANSFORM_FLAG_SHARABLE};

} // namespace dlstreamer
