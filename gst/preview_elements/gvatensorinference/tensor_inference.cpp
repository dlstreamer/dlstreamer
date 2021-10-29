/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <assert.h>

#include "config.h"

#include <cldnn/cldnn_config.hpp>
#include <gpu/gpu_params.hpp>
#include <ie_compound_blob.h>
#include <ie_plugin_config.hpp>

#include <safe_arithmetic.hpp>
#include <utils.h>

#include <inference_backend/logger.h>

#include "tensor_inference.hpp"

/* TODO: move to file for utilities, e.g. create file in common */
namespace {

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

    for (const auto &info : _network.getInputsInfo())
        _input_info.emplace_back(TensorLayerDesc::FromIeDesc(info.second->getTensorDesc(), info.first));
    for (const auto &info : _network.getOutputsInfo())
        _output_info.emplace_back(TensorLayerDesc::FromIeDesc(info.second->getTensorDesc(), info.first));
    std::transform(_output_info.begin(), _output_info.end(), std::back_inserter(_output_sizes),
                   [](const TensorLayerDesc &desc) { return desc.size; });
}

void TensorInference::Init(const std::string &device, size_t num_requests, const std::string &ie_config,
                           const PreProcInfo &preproc, const ImageInfo &image) {
    std::lock_guard<std::mutex> lock(_init_mutex);

    if (_is_initialized) {
        return;
    }

    ConfigurePreProcessing(preproc, image);

    if (_vaapi_surface_sharing_enabled && device != "GPU")
        throw std::runtime_error("Surface sharing is supported only on GPU device plugin");

    auto inference_config = Utils::stringToMap(ie_config);
    if (device == "CPU") {
        /* set cpu_throughput_streamer to auto */
        if (inference_config.find("CPU_THROUGHPUT_STREAMS") == inference_config.end()) {
            inference_config["CPU_THROUGHPUT_STREAMS"] = "CPU_THROUGHPUT_AUTO";
        }
    } else if (device == "GPU") {
        if (inference_config.find("GPU_THROUGHPUT_STREAMS") == inference_config.end()) {
            inference_config["GPU_THROUGHPUT_STREAMS"] = "GPU_THROUGHPUT_AUTO";
        }
    }

    GVA_INFO("Loading network ...");
#ifdef ENABLE_VAAPI
    if (_vaapi_surface_sharing_enabled) {
        using namespace InferenceEngine;

        if (!preproc.va_display)
            throw std::runtime_error("Can't create GPU context: VADisplay is null");

        InferenceEngine::ParamMap context_params = {
            {GPU_PARAM_KEY(CONTEXT_TYPE), GPU_PARAM_VALUE(VA_SHARED)},
            {GPU_PARAM_KEY(VA_DEVICE), static_cast<InferenceEngine::gpu_handle_param>(preproc.va_display)}};
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
    GVA_INFO("Loading network -> OK");

    if (num_requests == 0) {
        /* executable_network.GetMetric(...).as<size_t>() results in an error and the default
         * value is returned, which causes the perf degradation. We should use unsigned int here */
        num_requests = _executable_net.GetMetric(InferenceEngine::Metrics::METRIC_OPTIMAL_NUMBER_OF_INFER_REQUESTS)
                           .as<unsigned int>() +
                       1; // One additional for pre-processing parallelization with inference
    }

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

    _num_requests = num_requests;
    _is_initialized = true;
}

bool TensorInference::IsInitialized() const {
    std::lock_guard<std::mutex> lock(_init_mutex);
    return _is_initialized;
}

void TensorInference::ConfigurePreProcessing(const PreProcInfo &preproc, const ImageInfo &image) {
    assert(!_is_initialized);

    auto inputs_info = *_network.getInputsInfo().begin();
    inputs_info.second->getPreProcess().setResizeAlgorithm(preproc.resize_alg);
    inputs_info.second->getPreProcess().setColorFormat(preproc.color_format);

    if (!image) {
        GVA_INFO("TensorInference: external pre-processing");

        // TODO: find better way to understand pre-process type
        if (preproc.va_display) {
            // inputs_info.second->setLayout(InferenceEngine::Layout::NCHW);
            _vaapi_surface_sharing_enabled = true;
            GVA_INFO("TensorInference: VAAPI surface sharing");
        }
        return;
    }
    _image_info = image;
    _pre_proc_info = preproc;

    if (image.memory_type == InferenceBackend::MemoryType::SYSTEM) {
        _ie_preproc_enabled = true;
        GVA_INFO("TensorInference: IE pre-processing: ");
    }
}

const std::vector<TensorLayerDesc> &TensorInference::GetTensorInputInfo() const {
    return _input_info;
}

const std::vector<TensorLayerDesc> &TensorInference::GetTensorOutputInfo() const {
    return _output_info;
}

const std::vector<size_t> &TensorInference::GetTensorOutputSizes() const {
    return _output_sizes;
}

std::string TensorInference::GetModelName() const {
    return _network.getName();
}

size_t TensorInference::GetRequestsNum() const {
    return _num_requests;
}

/**
 * @brief Runs async inference on given input memory.
 *
 * @param input_mem - data to run inference on
 * @param output_mem - memory where output blob will be set
 * @param user_callback - callback called on inference completion
 */
void TensorInference::InferAsync(const std::shared_ptr<FrameData> input, std::shared_ptr<FrameData> output,
                                 CompletionCallback completion_callback, const RoiRect &roi) {
    Request::Ptr req = PrepareRequest(input, output, roi);
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
                                                              std::shared_ptr<FrameData> output, const RoiRect &roi) {
    TensorInference::Request::Ptr request;
    {
        ITT_TASK("Waiting free request");
        request = _free_requests.pop();
    }

    SetInputBlob(request, input, roi);
    SetOutputBlob(request, output);

    return request;
}

bool TensorInference::IsRunning() const {
    return _free_requests.size() < _num_requests;
}

void TensorInference::SetInputBlob(TensorInference::Request::Ptr request, const std::shared_ptr<FrameData> frame_data,
                                   const RoiRect &roi) {
    ITT_TASK("PREPARE INPUT BLOB");
    if (!frame_data)
        throw std::invalid_argument("Failed to set input buffer: FrameData is null");

    InferenceEngine::Blob::Ptr blob;
    // TODO: Will we handle multiple inputs info ??
    auto first_input_info = *_network.getInputsInfo().begin();

    if (_vaapi_surface_sharing_enabled) {
        blob = this->MakeNV12VaapiBlob(frame_data);
    } else {
        auto tensor_desc = first_input_info.second->getTensorDesc();
        switch (_pre_proc_info.color_format) {
        case InferenceEngine::I420:
            blob = MakeI420Blob(frame_data, tensor_desc, roi);
            break;
        case InferenceEngine::NV12:
            blob = MakeNV12Blob(frame_data, tensor_desc, roi);
            break;
        default:
            blob = MakeBGRBlob(frame_data, tensor_desc, roi);
            break;
        }
    }

    request->infer_req->SetBlob(first_input_info.first, blob);
}

void TensorInference::SetOutputBlob(TensorInference::Request::Ptr request, std::shared_ptr<FrameData> frame_data) {
    ITT_TASK("PREPARE OUTPUT BLOB");
    if (!frame_data)
        throw std::invalid_argument("Failed to set output buffer: FrameData is null");

    // TODO: More accurate way to handle multiple outputs
    const auto &outputs = _network.getOutputsInfo();
    assert(outputs.size() == frame_data->GetPlanesNum() && "Model outputs and frame data planes don't match");

    auto index = 0u;
    for (const auto &p : outputs) {
        auto tensor_desc = p.second->getTensorDesc();
        auto blob = MakeBlob(tensor_desc, frame_data->GetPlane(index));
        request->infer_req->SetBlob(p.first, blob);
        index++;
    }
}

InferenceEngine::Blob::Ptr TensorInference::MakeBlob(const InferenceEngine::TensorDesc &tensor_desc,
                                                     uint8_t *data) const {
    assert(data && "Expected valid data pointer");

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

InferenceEngine::Blob::Ptr TensorInference::MakeNV12VaapiBlob(const std::shared_ptr<FrameData> &frame_data) const {
    assert(frame_data && "Expected valid FrameData pointer");
#ifdef ENABLE_VAAPI
    using namespace InferenceEngine;

    assert(_executable_net.GetContext() && "Invalid remote context, can't create surface");
    const auto va_surface_id = frame_data->GetVaSurfaceID();
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

InferenceEngine::Blob::Ptr TensorInference::MakeNV12Blob(const std::shared_ptr<FrameData> &frame_data,
                                                         InferenceEngine::TensorDesc /*tensor_desc*/,
                                                         const RoiRect &roi) const {
    using namespace InferenceEngine;
    std::vector<size_t> NHWC = {0, 2, 3, 1};
    std::vector<size_t> dimOffsets = {0, 0, 0, 0};
    const size_t imageWidth = safe_convert<size_t>(frame_data->GetWidth());
    const size_t imageHeight = safe_convert<size_t>(frame_data->GetHeight());
    BlockingDesc memY(
        {1, imageHeight, imageWidth, 1}, NHWC, 0, dimOffsets,
        {frame_data->GetOffset(1) + frame_data->GetStride(0) * imageHeight / 2, frame_data->GetStride(0), 1, 1});
    BlockingDesc memUV(
        {1, imageHeight / 2, imageWidth / 2, 2}, NHWC, 0, dimOffsets,
        {frame_data->GetOffset(1) + frame_data->GetStride(0) * imageHeight / 2, frame_data->GetStride(1), 1, 1});
    TensorDesc planeY(InferenceEngine::Precision::U8, {1, 1, imageHeight, imageWidth}, memY);
    TensorDesc planeUV(InferenceEngine::Precision::U8, {1, 2, imageHeight / 2, imageWidth / 2}, memUV);

    auto blobY = MakeBlob(planeY, frame_data->GetPlane(0));
    auto blobUV = MakeBlob(planeUV, frame_data->GetPlane(1));
    if (!blobY || !blobUV)
        throw std::runtime_error("Failed to create blob for Y or UV plane");

    if (roi) {
        ROI crop_roi_y({
            0,
            safe_convert<size_t>(((roi.x & 0x1) ? roi.x - 1 : roi.x)),
            safe_convert<size_t>(((roi.y & 0x1) ? roi.y - 1 : roi.y)),
            safe_convert<size_t>(((roi.w & 0x1) ? roi.w - 1 : roi.w)),
            safe_convert<size_t>(((roi.h & 0x1) ? roi.h - 1 : roi.h)),
        });
        ROI crop_roi_uv({0, safe_convert<size_t>(roi.x / 2), safe_convert<size_t>(roi.y / 2),
                         safe_convert<size_t>(roi.w / 2), safe_convert<size_t>(roi.h / 2)});

        blobY = make_shared_blob(blobY, crop_roi_y);
        blobUV = make_shared_blob(blobUV, crop_roi_uv);
    }

    return make_shared_blob<NV12Blob>(blobY, blobUV);
}

InferenceEngine::Blob::Ptr TensorInference::MakeI420Blob(const std::shared_ptr<FrameData> &frame_data,
                                                         InferenceEngine::TensorDesc /*tensor_desc*/,
                                                         const RoiRect &roi) const {
    using namespace InferenceEngine;
    std::vector<size_t> NHWC = {0, 2, 3, 1};
    std::vector<size_t> dimOffsets = {0, 0, 0, 0};
    const size_t image_width = static_cast<size_t>(frame_data->GetWidth());
    const size_t image_height = static_cast<size_t>(frame_data->GetHeight());
    BlockingDesc memY(
        {1, image_height, image_width, 1}, NHWC, 0, dimOffsets,
        {frame_data->GetOffset(1) + image_height * frame_data->GetStride(0) / 2, frame_data->GetStride(0), 1, 1});
    BlockingDesc memU(
        {1, image_height / 2, image_width / 2, 1}, NHWC, 0, dimOffsets,
        {frame_data->GetOffset(1) + image_height * frame_data->GetStride(0) / 2, frame_data->GetStride(1), 1, 1});
    BlockingDesc memV(
        {1, image_height / 2, image_width / 2, 1}, NHWC, 0, dimOffsets,
        {frame_data->GetOffset(1) + image_height * frame_data->GetStride(0) / 2, frame_data->GetStride(2), 1, 1});

    TensorDesc Y_plane_desc(InferenceEngine::Precision::U8, {1, 1, image_height, image_width}, memY);
    TensorDesc U_plane_desc(InferenceEngine::Precision::U8, {1, 1, image_height / 2, image_width / 2}, memU);
    TensorDesc V_plane_desc(InferenceEngine::Precision::U8, {1, 1, image_height / 2, image_width / 2}, memV);
    if (frame_data->GetPlanesNum() < 3)
        throw std::invalid_argument("Planes number for I420 image is less than 3");

    auto Y_plane_blob = MakeBlob(Y_plane_desc, frame_data->GetPlane(0));
    auto U_plane_blob = MakeBlob(U_plane_desc, frame_data->GetPlane(1));
    auto V_plane_blob = MakeBlob(V_plane_desc, frame_data->GetPlane(2));
    if (!Y_plane_blob || !U_plane_blob || !V_plane_blob)
        throw std::runtime_error("Failed to create blob for Y, or U, or V plane");

    if (roi) {
        ROI Y_roi({
            0,
            safe_convert<size_t>(((roi.x & 0x1) ? roi.x - 1 : roi.x)),
            safe_convert<size_t>(((roi.y & 0x1) ? roi.y - 1 : roi.y)),
            safe_convert<size_t>(((roi.w & 0x1) ? roi.w - 1 : roi.w)),
            safe_convert<size_t>(((roi.h & 0x1) ? roi.h - 1 : roi.h)),
        });
        ROI U_V_roi({0, safe_convert<size_t>(roi.x / 2), safe_convert<size_t>(roi.y / 2),
                     safe_convert<size_t>(roi.w / 2), safe_convert<size_t>(roi.h / 2)});

        Y_plane_blob = make_shared_blob(Y_plane_blob, Y_roi);
        U_plane_blob = make_shared_blob(U_plane_blob, U_V_roi);
        V_plane_blob = make_shared_blob(V_plane_blob, U_V_roi);
    }

    return make_shared_blob<I420Blob>(Y_plane_blob, U_plane_blob, V_plane_blob);
}

InferenceEngine::Blob::Ptr TensorInference::MakeBGRBlob(const std::shared_ptr<FrameData> &frame_data,
                                                        InferenceEngine::TensorDesc tensor_desc,
                                                        const RoiRect &roi) const {
    if (_ie_preproc_enabled) {
        tensor_desc.setLayout(InferenceEngine::Layout::NHWC);
        tensor_desc.setDims({1, static_cast<uint32_t>(_image_info.channels), _image_info.height, _image_info.width});
    }

    auto blob = MakeBlob(tensor_desc, frame_data->GetPlane(0));
    if (roi) {
        InferenceEngine::ROI blob_roi({0, (size_t)roi.x, (size_t)roi.y, (size_t)roi.w, (size_t)roi.h});
        blob = InferenceEngine::make_shared_blob(blob, blob_roi);
    }
    return blob;
}

void TensorInference::onInferCompleted(Request::Ptr request, InferenceEngine::StatusCode code) {
    std::string error;
    if (code != InferenceEngine::StatusCode::OK) {
        error = "Return status: " + get_message(code);
    }
    auto cb = std::move(request->completion_callback);
    _free_requests.push(request);
    cb(error);
    _request_processed.notify_all();
}

void TensorInference::Flush() {
    // because Flush can execute by several threads for one InferenceImpl instance
    // it must be synchronous.
    std::unique_lock<std::mutex> flush_lk(_flush_mutex);
    // wait_for unlocks flush_mutex until we get notify
    _request_processed.wait_for(flush_lk, std::chrono::seconds(1), [this] { return !IsRunning(); });
}
