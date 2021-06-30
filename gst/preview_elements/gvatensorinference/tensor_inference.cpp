/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <assert.h>

#include <gst/gst.h>

#include "config.h"

#include <cldnn/cldnn_config.hpp>
#include <gpu/gpu_params.hpp>
#include <ie_compound_blob.h>

#include <gva_custom_meta.hpp>

#include "tensor_inference.hpp"

/* TODO: move to file for utilities, e.g. create file in common */
namespace {

constexpr short FP32_BYTES = 4;

// FIXME: copypaste function, get rid of it
std::string get_message(InferenceEngine::StatusCode code) {
    switch (code) {

    case InferenceEngine::StatusCode::OK:
        return std::string("OK");

    case InferenceEngine::StatusCode::GENERAL_ERROR:
        return std::string("GENERAL_ERROR");

    case InferenceEngine::StatusCode::NOT_IMPLEMENTED:
        return std::string("NOT_IMPLEMENTED");

    case InferenceEngine::StatusCode::NETWORK_NOT_LOADED:
        return std::string("NETWORK_NOT_LOADED");

    case InferenceEngine::StatusCode::PARAMETER_MISMATCH:
        return std::string("PARAMETER_MISMATCH");

    case InferenceEngine::StatusCode::NOT_FOUND:
        return std::string("NOT_FOUND");

    case InferenceEngine::StatusCode::OUT_OF_BOUNDS:
        return std::string("OUT_OF_BOUNDS");

    case InferenceEngine::StatusCode::UNEXPECTED:
        return std::string("UNEXPECTED");

    case InferenceEngine::StatusCode::REQUEST_BUSY:
        return std::string("REQUEST_BUSY");

    case InferenceEngine::StatusCode::RESULT_NOT_READY:
        return std::string("RESULT_NOT_READY");

    case InferenceEngine::StatusCode::NOT_ALLOCATED:
        return std::string("NOT_ALLOCATED");

    case InferenceEngine::StatusCode::INFER_NOT_STARTED:
        return std::string("INFER_NOT_STARTED");

    case InferenceEngine::StatusCode::NETWORK_NOT_READ:
        return std::string("NETWORK_NOT_READ");

    default:
        return std::string("UNKNOWN_IE_STATUS_CODE");
    }
}

std::map<std::string, std::string> parse_config(const std::string &s, char rec_delim = ',', char kv_delim = '=') {
    std::string key, val;
    std::istringstream iss(s);
    std::map<std::string, std::string> m;

    while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
        m.emplace(std::move(key), std::move(val));
    }

    return m;
}

} /* anonymous namespace */

TensorInference::TensorInference(const std::string &model_path) {
    _network = _ie.ReadNetwork(model_path);

    // TODO: investigate what it is used for even in current impl in main
    assert(_network.getInputsInfo().size() == 1);
    auto inputs_info = *_network.getInputsInfo().begin();
    auto layout = inputs_info.second->getTensorDesc().getLayout();
    /* TODO: U8 precision must be set for any image */
    if (layout == InferenceEngine::Layout::NHWC || layout == InferenceEngine::Layout::NCHW) {
        inputs_info.second->setPrecision(InferenceEngine::Precision::U8);
    }
}

void TensorInference::Init(const std::string &device, size_t num_requests, const std::string &ie_config,
                           const PreProcInfo &preproc, const ImageInfo &image) {
    if (_is_initialized) {
        return;
    }

    ConfigurePreProcessing(preproc, image);

    if (_vaapi_surface_sharing_enabled && device != "GPU")
        throw std::runtime_error("Surface sharing is supported only on GPU device plugin");

    auto inference_config = parse_config(ie_config);
#ifdef ENABLE_VAAPI
    if (_vaapi_surface_sharing_enabled) {
        if (!_image_info.va_display)
            throw std::runtime_error("Can't create GPU context: VADisplay is null");

        InferenceEngine::ParamMap context_params = {
            {InferenceEngine::GPU_PARAM_KEY(CONTEXT_TYPE), InferenceEngine::GPU_PARAM_VALUE(VA_SHARED)},
            {InferenceEngine::GPU_PARAM_KEY(VA_DEVICE),
             static_cast<InferenceEngine::gpu_handle_param>(_image_info.va_display)}};
        auto context = _ie.CreateContext(device, context_params);
        // This is a temporary workround to provide a compound blob instead of a remote one
        inference_config[InferenceEngine::CLDNNConfigParams::KEY_CLDNN_NV12_TWO_INPUTS] =
            InferenceEngine::PluginConfigParams::YES;
        // Surface sharing works only with GPU_THROUGHPUT_STREAMS equal to default value ( = 1)
        inference_config.erase("GPU_THROUGHPUT_STREAMS");
        _executable_net = _ie.LoadNetwork(_network, context, inference_config);
    } else {
#endif
        _executable_net = _ie.LoadNetwork(_network, device, inference_config);
#ifdef ENABLE_VAAPI
    }
#endif

    for (size_t i = 0; i < num_requests; ++i) {
        Request::Ptr request = std::make_shared<Request>();
        request->infer_req = _executable_net.CreateInferRequestPtr();
        request->infer_req
            ->SetCompletionCallback<std::function<void(InferenceEngine::InferRequest, InferenceEngine::StatusCode)>>(
                [this, request](InferenceEngine::InferRequest /* request */, InferenceEngine::StatusCode code) {
                    onInferCompleted(request, code);
                });
        _free_requests.push(request);
    }

    _is_initialized = true;
}

void TensorInference::ConfigurePreProcessing(const PreProcInfo &preproc, const ImageInfo &image) {
    assert(!_is_initialized);

    if (!image) {
        return;
    }

    auto inputs_info = *_network.getInputsInfo().begin();
    inputs_info.second->getPreProcess().setResizeAlgorithm(preproc.resize_alg);
    inputs_info.second->getPreProcess().setColorFormat(preproc.color_format);
    _image_info = image;
    _pre_proc_info = preproc;

    if (image.memory_type == InferenceBackend::MemoryType::SYSTEM) {
        _ie_preproc_enabled = true;
    } else {
        _vaapi_surface_sharing_enabled = true;
    }
}

TensorInference::TensorInputInfo TensorInference::GetTensorInputInfo() const {
    // Lazy init
    if (_input_info)
        return _input_info;

    // TODO: there may be multiple inputs and outputs ?
    for (auto p : _network.getInputsInfo()) {
        auto tensor_description = p.second->getTensorDesc();
        _input_info.precision = tensor_description.getPrecision();
        _input_info.layout = tensor_description.getLayout();
        _input_info.dims = tensor_description.getDims();

        switch (_input_info.layout) {
        case InferenceEngine::Layout::NHWC:
            _input_info.height = _input_info.dims[1];
            _input_info.width = _input_info.dims[2];
            _input_info.channels = _input_info.dims[3];
            break;
        case InferenceEngine::Layout::NCHW:
            _input_info.channels = _input_info.dims[1];
            _input_info.height = _input_info.dims[2];
            _input_info.width = _input_info.dims[3];
            break;
        case InferenceEngine::Layout::CHW:
            _input_info.channels = _input_info.dims[0];
            _input_info.height = _input_info.dims[1];
            _input_info.width = _input_info.dims[2];
            break;
        case InferenceEngine::Layout::NC:
            _input_info.channels = _input_info.dims[1];
            break;
        default:
            break;
        }
    }

    if (!_input_info)
        throw std::runtime_error("Couldn't get image inputs information for model!");

    return _input_info;
}

// TODO: very similar method as for inputInfo
TensorInference::TensorOutputInfo TensorInference::GetTensorOutputInfo() const {
    // Lazy init
    if (_output_info)
        return _output_info;

    for (auto p : _network.getOutputsInfo()) {
        auto tensor_description = p.second->getTensorDesc();
        _output_info.precision = tensor_description.getPrecision();
        _output_info.layout = tensor_description.getLayout();
        _output_info.dims = tensor_description.getDims();

        switch (_output_info.layout) {
        case InferenceEngine::Layout::NHWC:
            _output_info.height = _output_info.dims[1];
            _output_info.width = _output_info.dims[2];
            _output_info.channels = _output_info.dims[3];
            break;
        case InferenceEngine::Layout::NCHW:
            _output_info.channels = _output_info.dims[1];
            _output_info.height = _output_info.dims[2];
            _output_info.width = _output_info.dims[3];
            break;
        case InferenceEngine::Layout::CHW:
            _output_info.channels = _output_info.dims[0];
            _output_info.height = _output_info.dims[1];
            _output_info.width = _output_info.dims[2];
            break;
        case InferenceEngine::Layout::NC:
            _output_info.channels = _output_info.dims[1];
            break;
        default:
            break;
        }

        for (const auto &i : _output_info.dims)
            _output_info.size *= i;

        if (_output_info.precision == InferenceEngine::Precision::FP32) {
            _output_info.size *= FP32_BYTES;
        }
    }

    if (!_output_info)
        throw std::runtime_error("Couldn't get image outputs information for model!");

    return _output_info;
}

/**
 * @brief Runs async inference on given input memory.
 *
 * @param input_mem - data to run inference on
 * @param output_mem - memory where output blob will be set
 * @param user_callback - callback called on inference completion
 */
void TensorInference::InferAsync(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output,
                                 CompletionCallback completion_callback) {
    Request::Ptr req = PrepareRequest(input, output);
    req->completion_callback = completion_callback;

    req->infer_req->StartAsync();
}

/**
 * @brief Prepares inference request by setting input/output blobs
 *
 * @param input_mem - data to run inference on
 * @param output_mem - memory where output blob will be set
 */
TensorInference::Request::Ptr TensorInference::PrepareRequest(const std::shared_ptr<FrameData> input,
                                                              std::shared_ptr<FrameData> output) {

    auto request = _free_requests.pop();

    SetInputBlob(request, input);
    SetOutputBlob(request, output);

    return request;
}

void TensorInference::SetInputBlob(TensorInference::Request::Ptr request, const std::shared_ptr<FrameData> frame_data) {
    InferenceEngine::Blob::Ptr blob;
    // TODO: Will we handle multiple inputs info ??
    auto first_input_info = *_network.getInputsInfo().begin();

    if (_vaapi_surface_sharing_enabled) {
        blob = this->MakeNV12VaapiBlob(frame_data);
    } else {
        auto tensor_desc = first_input_info.second->getTensorDesc();

        if (_ie_preproc_enabled) {
            tensor_desc.setLayout(InferenceEngine::Layout::NHWC);
            tensor_desc.setDims(
                {1, static_cast<uint32_t>(_image_info.channels), _image_info.height, _image_info.width});
        }

        blob = this->MakeBlob(tensor_desc, frame_data->GetPlane(0));
    }

    request->infer_req->SetBlob(first_input_info.first, blob);
}

void TensorInference::SetOutputBlob(TensorInference::Request::Ptr request, std::shared_ptr<FrameData> frame_data) {
    // TODO: Will we handle multiple outputs info ??
    auto first_output_info = *_network.getOutputsInfo().begin();
    auto tensor_desc = first_output_info.second->getTensorDesc();

    InferenceEngine::Blob::Ptr blob = this->MakeBlob(tensor_desc, frame_data->GetPlane(0));

    request->infer_req->SetBlob(first_output_info.first, blob);
}

InferenceEngine::Blob::Ptr TensorInference::MakeBlob(const InferenceEngine::TensorDesc &tensor_desc, uint8_t *data) {
    auto precision = tensor_desc.getPrecision();

    switch (precision) {
    case InferenceEngine::Precision::U8:
        return InferenceEngine::make_shared_blob<uint8_t>(tensor_desc, data);
    case InferenceEngine::Precision::FP32:
        return InferenceEngine::make_shared_blob<float>(tensor_desc, reinterpret_cast<float *>(data));
    default:
        throw std::invalid_argument("Failed to create Blob: InferenceEngine::Precision " + std::to_string(precision) +
                                    " is not supported");
    }
}

InferenceEngine::Blob::Ptr TensorInference::MakeNV12VaapiBlob(const std::shared_ptr<FrameData> frame_data) {
#ifdef ENABLE_VAAPI
    using namespace InferenceEngine;

    assert(_executable_net.GetContext() && "Invalid remote context, can't create surface");
    const uint32_t VASURFACE_INVALID_ID = 0xffffffff;
    const auto va_surface_id = frame_data->GetVaMemInfo().va_surface_id;
    if (va_surface_id == VASURFACE_INVALID_ID)
        throw std::runtime_error("Incorrect VA surface");

    auto create_vaapi_blob = [this, va_surface_id](const TensorDesc &tensor_desc, uint32_t plane_num) {
        return _executable_net.GetContext()->CreateBlob(tensor_desc,
                                                        {{GPU_PARAM_KEY(SHARED_MEM_TYPE), GPU_PARAM_VALUE(VA_SURFACE)},
                                                         {GPU_PARAM_KEY(DEV_OBJECT_HANDLE), va_surface_id},
                                                         {GPU_PARAM_KEY(VA_PLANE), plane_num}});
    };

    TensorDesc y_desc(InferenceEngine::Precision::U8, {1, 1, frame_data->GetHeight(), frame_data->GetWidth()},
                      InferenceEngine::Layout::NHWC);
    TensorDesc uv_desc(InferenceEngine::Precision::U8, {1, 2, frame_data->GetHeight() / 2, frame_data->GetWidth() / 2},
                       InferenceEngine::Layout::NHWC);
    auto blob_y = create_vaapi_blob(y_desc, 0);
    auto blob_uv = create_vaapi_blob(uv_desc, 1);
    if (!blob_y || !blob_uv)
        throw std::runtime_error("Failed to create blob for Y or UV plane");
    return make_shared_blob<NV12Blob>(blob_y, blob_uv);
#else
    (void)frame_data;
    assert(false && "Attempt to use surface sharing but project was built without vaapi support.");
    return nullptr;
#endif
}

void TensorInference::onInferCompleted(Request::Ptr request, InferenceEngine::StatusCode code) {
    std::string error;
    if (code != InferenceEngine::StatusCode::OK) {
        error = "Return status: " + get_message(code);
    }

    request->completion_callback(error);
    _free_requests.push(request);
}
