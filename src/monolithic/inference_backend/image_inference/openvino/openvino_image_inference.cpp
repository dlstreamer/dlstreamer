/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "openvino_image_inference.h"

#include "config.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "model_loader.h"
#include "openvino_blob_wrapper.h"
#include "safe_arithmetic.hpp"
#include "utils.h"
#include "wrap_image.h"

#ifdef ENABLE_VPUX
#include <ie_remote_context.hpp>
#include <vpux/kmb_params.hpp>
#endif

#ifdef ENABLE_VAAPI
#include <dlstreamer/vaapi/context.h>
#include <gpu/gpu_params.hpp>
#ifdef ENABLE_GPU_TILE_AFFINITY
#include "vaapi_utils.h"
#endif
#endif

#include <ie_compound_blob.h>
#include <inference_engine.hpp>

#include <chrono>
#include <functional>
#include <stdio.h>
#include <thread>

#include <core_singleton.h>

using namespace InferenceBackend;

namespace {

inline size_t GetTensorSize(InferenceEngine::TensorDesc desc) {
    // TODO: merge with GetUnbatchedSizeInBytes to avoid double implementation
    auto dims = desc.getDims();
    size_t size = 1;
    for (auto dim : dims)
        size *= dim;
    switch (desc.getPrecision()) {
    case InferenceEngine::Precision::U8:
        return size;
    case InferenceEngine::Precision::FP32:
        return size * sizeof(float);
    default:
        throw std::invalid_argument("Failed to get tensor size for tensor with " + std::to_string(desc.getPrecision()) +
                                    " InferenceEngine::Precision");
    }
}

inline std::vector<std::string> split(const std::string &s, char delimiter) {
    std::string token;
    std::istringstream tokenStream(s);
    std::vector<std::string> tokens;
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::tuple<InferenceEngine::Blob::Ptr, InferenceBackend::Allocator::AllocContext *>
allocateBlob(const InferenceEngine::TensorDesc &tensor_desc, Allocator *allocator) {
    assert(allocator && "Allocator is null");

    try {
        void *buffer_ptr = nullptr;
        InferenceBackend::Allocator::AllocContext *alloc_context = nullptr;
        const size_t size = GetTensorSize(tensor_desc);
        allocator->Alloc(size, buffer_ptr, alloc_context);
        InferenceEngine::Blob::Ptr blob;
        if (buffer_ptr && alloc_context) {
            switch (tensor_desc.getPrecision()) {
            case InferenceEngine::Precision::U8:
                blob = InferenceEngine::make_shared_blob<uint8_t>(tensor_desc, reinterpret_cast<uint8_t *>(buffer_ptr));
                break;
            case InferenceEngine::Precision::FP32:
                blob = InferenceEngine::make_shared_blob<float>(tensor_desc, reinterpret_cast<float *>(buffer_ptr));
                break;
            default:
                throw std::invalid_argument("Failed to create Blob: InferenceEngine::Precision " +
                                            std::to_string(tensor_desc.getPrecision()) + " is not supported");
            }
        } else {
            throw std::runtime_error("Failed to allocate memory for blob");
        }
        return std::make_tuple(blob, alloc_context);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to allocate InferenceEngine Blob"));
    }
}

size_t optimalNireq(const InferenceEngine::ExecutableNetwork &executable_network) {
    size_t nireq = 0;
    try {
        /* executable_network.GetMetric(...).as<size_t>() results in an error and the default
         * value is returned, which causes the perf degradation. We should use unsigned int here */
        nireq = executable_network.GetMetric(InferenceEngine::Metrics::METRIC_OPTIMAL_NUMBER_OF_INFER_REQUESTS)
                    .as<unsigned int>() +
                1; // One additional for pre-processing parallelization with inference
        GVA_INFO("Setting the optimal number of inference requests: nireq=%lu", nireq);
    } catch (const std::exception &e) {
        GVA_ERROR(
            "Failed to get optimal number of inference requests: %s\nNumber of inference requests will fallback to 1",
            e.what());
        return 1;
    }
    return nireq;
}

template <class T>
Image FillImage(const InferenceEngine::Blob::Ptr &blob, const InferenceEngine::SizeVector &dims, const size_t index) {
    assert(blob && "IE Blob is null");

    Image image = Image();
    image.width = safe_convert<uint32_t>(dims[3]);
    image.height = safe_convert<uint32_t>(dims[2]);
    if (index >= dims[0]) {
        throw std::out_of_range("Image index is out of range in batch blob");
    }
    size_t plane_size = image.width * image.height * sizeof(T);
    size_t buffer_offset = safe_mul(safe_mul(index, plane_size), dims[1]);

    image.planes[0] = blob->buffer().as<uint8_t *>() + buffer_offset;
    image.planes[1] = image.planes[0] + plane_size;
    image.planes[2] = image.planes[1] + plane_size;
    image.planes[3] = nullptr;

    image.stride[0] = image.width;
    image.stride[1] = image.width;
    image.stride[2] = image.width;
    image.stride[3] = 0;
    return image;
}

Image MapBlobBufferToImage(InferenceEngine::Blob::Ptr blob, size_t batch_index) {
    GVA_DEBUG("enter");
    ITT_TASK(__FUNCTION__);
    assert(blob && "IE Blob is null");

    auto desc = blob->getTensorDesc();
    auto dims = desc.getDims();
    if (desc.getLayout() != InferenceEngine::Layout::NCHW) {
        throw std::runtime_error("Unsupported layout");
    }
    Image image = Image();
    switch (desc.getPrecision()) {
    case InferenceEngine::Precision::FP32:
        image = FillImage<float>(blob, desc.getDims(), batch_index);
        image.format = FourCC::FOURCC_RGBP_F32;
        break;
    case InferenceEngine::Precision::U8:
        image = FillImage<uint8_t>(blob, desc.getDims(), batch_index);
        image.format = FourCC::FOURCC_RGBP;
        break;
    default:
        throw std::runtime_error("Unsupported precision");
        break;
    }
    return image;
}

const InputImageLayerDesc::Ptr
getImagePreProcInfo(const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) {
    const auto image_it = input_preprocessors.find("image");
    if (image_it != input_preprocessors.cend()) {
        const auto description = image_it->second;
        if (description) {
            return description->input_image_preroc_params;
        }
    }

    return nullptr;
}

} // namespace

OpenVINOImageInference::~OpenVINOImageInference() {
    GVA_DEBUG("Image Inference destruct");
    Close();
}

std::string getErrorMsg(InferenceEngine::StatusCode code) {
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
        break;
    }

    return std::string("UNKNOWN_IE_STATUS_CODE");
}

void OpenVINOImageInference::SetCompletionCallback(std::shared_ptr<BatchRequest> &batch_request) {
    assert(batch_request && "Batch request is null");

    auto completion_callback = [this, batch_request](InferenceEngine::InferRequest, InferenceEngine::StatusCode code) {
        try {
            ITT_TASK("completion_callback_lambda");

            if (code != InferenceEngine::StatusCode::OK) {
                GVA_ERROR("Inference request failed with code: %d (%s)", code, getErrorMsg(code).c_str());
                this->handleError(batch_request->buffers);
            } else {
                this->WorkingFunction(batch_request);
            }

            FreeRequest(batch_request);
        } catch (const std::exception &e) {
            GVA_ERROR("An error occurred at inference request completion callback:\n%s",
                      Utils::createNestedErrorMsg(e).c_str());
        }
    };
    batch_request->infer_request
        ->SetCompletionCallback<std::function<void(InferenceEngine::InferRequest, InferenceEngine::StatusCode)>>(
            completion_callback);
}

void OpenVINOImageInference::SetBlobsToInferenceRequest(
    const std::map<std::string, InferenceEngine::TensorDesc> &layers, std::shared_ptr<BatchRequest> &batch_request,
    Allocator *allocator) {
    for (const auto &layer : layers) {
        InferenceBackend::Allocator::AllocContext *alloc_context = nullptr;
        InferenceEngine::Blob::Ptr blob;
        std::tie(blob, alloc_context) = allocateBlob(layer.second, allocator);
        batch_request->infer_request->SetBlob(layer.first, blob);
        batch_request->alloc_context.push_back(alloc_context);
    }
}

std::unique_ptr<WrapImageStrategy::General>
OpenVINOImageInference::CreateWrapImageStrategy(MemoryType memory_type, const std::string &device,
                                                const InferenceEngine::RemoteContext::Ptr &remote_context) {
    std::unique_ptr<WrapImageStrategy::General> wrap_strategy;
    switch (memory_type) {
    case MemoryType::SYSTEM: {
        if (device.find("VPUX") != device.npos)
            wrap_strategy.reset(new WrapImageStrategy::VPUX(remote_context));
        else
            wrap_strategy.reset(new WrapImageStrategy::General());
        break;
    }
    case MemoryType::VAAPI: {
        wrap_strategy.reset(new WrapImageStrategy::GPU(remote_context));
        break;
    }
    default:
        throw std::invalid_argument("Unsupported memory type");
    }
    return wrap_strategy;
}

OpenVINOImageInference::OpenVINOImageInference(const InferenceBackend::InferenceConfig &config, Allocator *allocator,
                                               dlstreamer::ContextPtr context, CallbackFunc callback,
                                               ErrorHandlingFunc error_handler, MemoryType memory_type)
    : allocator(allocator), context_(context), memory_type(memory_type), callback(callback), handleError(error_handler),
      batch_size(std::stoi(config.at(KEY_BASE).at(KEY_BATCH_SIZE))), requests_processing_(0U) {

    try {
        const std::map<std::string, std::string> &base_config = config.at(KEY_BASE);

        InferenceEngine::RemoteContext::Ptr remote_context = CreateRemoteContext(config);

        const std::string &model = base_config.at(KEY_MODEL);
        builder = ModelLoader::is_compile_model(model)
                      ? std::unique_ptr<EntityBuilder>(new CompiledBuilder(config, model, remote_context))
                      : std::unique_ptr<EntityBuilder>(new IrBuilder(config, model, remote_context));
        if (not builder)
            throw std::runtime_error("Failed to create DL model loader");
        network = builder->createNetwork();
        nireq = std::stoi(base_config.at(KEY_NIREQ));

        InferenceEngine::ExecutableNetwork executable_network;
        std::tie(pre_processor, executable_network, image_layer) = builder->createPreProcAndExecutableNetwork(network);

        NetworkReferenceWrapper network_ref(network, executable_network);
        model_name = builder->getNetworkName(network_ref);

        inputs = executable_network.GetInputsInfo();
        outputs = executable_network.GetOutputsInfo();
        std::map<std::string, InferenceEngine::TensorDesc> layers;
        for (auto input : inputs) {
            layers[input.first] = input.second->getTensorDesc();
        }
        for (auto output : outputs) {
            layers[output.first] = output.second->getTensorDesc();
        }

        if (nireq == 0) {
            nireq = safe_convert<int>(optimalNireq(executable_network));
        }

        for (int i = 0; i < nireq; i++) {
            std::shared_ptr<BatchRequest> batch_request = std::make_shared<BatchRequest>();
            batch_request->infer_request = executable_network.CreateInferRequestPtr();
            SetCompletionCallback(batch_request);
            if (allocator) {
                SetBlobsToInferenceRequest(layers, batch_request, allocator);
            }
            freeRequests.push(batch_request);
        }
        wrap_strategy = CreateWrapImageStrategy(memory_type, base_config.at(KEY_DEVICE), remote_context);

    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to construct OpenVINOImageInference"));
    }
}

void OpenVINOImageInference::FreeRequest(std::shared_ptr<BatchRequest> request) {
    const size_t buffer_size = request->buffers.size();
    request->buffers.clear();
    freeRequests.push(request);
    requests_processing_ -= buffer_size;
    request_processed_.notify_all();
}

InferenceEngine::RemoteContext::Ptr
OpenVINOImageInference::CreateRemoteContext(const InferenceBackend::InferenceConfig &config) {
    InferenceEngine::RemoteContext::Ptr remote_context;
    const std::string &device = config.at(KEY_BASE).at(KEY_DEVICE);

#ifdef ENABLE_VPUX
    std::string vpu_device_name;
    bool has_vpu_device_id = false;
    std::tie(has_vpu_device_id, vpu_device_name) = Utils::parseDeviceName(device);
    if (!vpu_device_name.empty()) {
        const std::string msg = "VPUX device defined as " + vpu_device_name;
        GVA_INFO(msg.c_str());

        const std::string base_device = "VPUX";
        std::string device = vpu_device_name;
        if (!has_vpu_device_id) {
            // Retrieve ID of the first available device
            std::vector<std::string> device_list =
                IeCoreSingleton::Instance().GetMetric(base_device, METRIC_KEY(AVAILABLE_DEVICES));
            if (!device_list.empty())
                device = device_list.at(0);
            // else device is already set to VPU-0
        }
        const InferenceEngine::ParamMap params = {{InferenceEngine::KMB_PARAM_KEY(DEVICE_ID), device}};
        remote_context = IeCoreSingleton::Instance().CreateContext(base_device, params);
    }
#endif

#ifdef ENABLE_VAAPI
    const bool is_gpu_device = device.rfind("GPU", 0) == 0;
    // There are 3 possible scenarios:
    // 1. memory_type == VAAPI: we are going to use the surface sharing mode and we have to provide
    // VADisplay to GPU plugin so it can work with VaSurfaces that we are going to submit via BLOBs.
    // 2.1. memory_type == SYSTEM and display is available: VAAPI serves only as pre-processing and
    // we don't have to provide VADisplay to the plugin in such case. However, by providing VADisplay,
    // we can make sure that the plugin will choose the same GPU for inference as for decoding by
    // vaapi elements.
    // 2.2. memory_type == SYSTEM and display is not available: user chose to perform decode on CPU
    // and inference on GPU. IE pre-processing is used in this case.
    // In cases 1 and 2.1 we don't need to specify GPU number (GPU.0, GPU.1) to achieve device affinity.
    // In case 2.2 user may choose desired GPU for inference. Currently matching GPU ID to actual
    // hardware is not possible due to OpenVINO™ API lacking.
    if (is_gpu_device && (memory_type == MemoryType::VAAPI || memory_type == MemoryType::SYSTEM)) {
        if (context_) {
            using namespace InferenceEngine;

            // TODO: Bug in OpenVINO™ 2021.4.X. Caused by using GPU_THROUGHPUT_STREAMS=GPU_THROUGHPUT_AUTO.
            // During CreateContext() call, IE creates queues for each of GPU_THROUGHPUT_STREAMS. By
            // default GPU_THROUGHPUT_STREAMS is set to 1, so IE creates only 1 queue. Then, during
            // LoadNetwork() call we pass GPU_THROUGHPUT_STREAMS=GPU_THROUGHPUT_AUTO and GPU plugin may
            // set number of streams to more than 1 (for example, 2). Then for each of the streams (2)
            // GPU plugin tries to get IE queue, but fails because number of streams is greater than
            // number of created queues.
            // Because of that user may get an error:
            //     Unable to create network with stream_id=1
            // Workaround for this is to set GPU_THROUGHPUT_STREAMS to IE prior to CreateContext() call.
            auto &ie_config = config.at(KEY_INFERENCE);
            auto it = ie_config.find(KEY_GPU_THROUGHPUT_STREAMS);
            if (it != ie_config.end())
                IeCoreSingleton::Instance().SetConfig({*it}, "GPU");

            auto va_display = context_->handle(dlstreamer::VAAPIContext::key::va_display);
            if (!va_display)
                throw std::runtime_error("Error getting va_display from context");

            InferenceEngine::ParamMap contextParams = {
                {GPU_PARAM_KEY(CONTEXT_TYPE), GPU_PARAM_VALUE(VA_SHARED)},
                {GPU_PARAM_KEY(VA_DEVICE), static_cast<InferenceEngine::gpu_handle_param>(va_display)}};

            // GPU tile affinity
            if (device == "GPU.x") {
#ifdef ENABLE_GPU_TILE_AFFINITY
                VaDpyWrapper dpyWrapper(va_display);
                int tile_id = dpyWrapper.currentSubDevice();
                // If tile_id is -1 (single-tile GPU is used or the driver doesn't support this feature)
                // then GPU plugin will choose the device and tile on its own and there will be no affinity
                contextParams.insert({GPU_PARAM_KEY(TILE_ID), tile_id});
#else
                GVA_WARNING("Current version of OpenVINO™ toolkit doesn't support tile affinity, version 2022.1 or "
                            "higher is required");
#endif
            }

            remote_context = IeCoreSingleton::Instance().CreateContext("GPU", contextParams);
        } else if (memory_type == MemoryType::VAAPI) {
            throw std::runtime_error("Display must be provided for GPU device with vaapi-surface-sharing backend");
        }
    }
#else
    UNUSED(device);
#endif

    return remote_context;
}

bool OpenVINOImageInference::IsQueueFull() {
    return freeRequests.empty();
}

void OpenVINOImageInference::SubmitImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                                                   const Image &src_img, const InputImageLayerDesc::Ptr &pre_proc_info,
                                                   const ImageTransformationParams::Ptr image_transform_info) {
    ITT_TASK(__FUNCTION__);
    if (not request or not request->infer_request)
        throw std::invalid_argument("InferRequest is absent");
    if (request->blob.empty())
        request->blob.push_back(request->infer_request->GetBlob(input_name));
    size_t batch_index = request->buffers.size();
    Image dst_img = MapBlobBufferToImage(request->blob[0], batch_index);
    if (src_img.planes[0] != dst_img.planes[0]) { // only convert if different buffers
        try {
            pre_processor->Convert(src_img, dst_img, pre_proc_info, image_transform_info);
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed while software frame preprocessing"));
        }
    }
}

void OpenVINOImageInference::BypassImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                                                   const Image &src_img, size_t batch_size) {
    ITT_TASK(__FUNCTION__);

    if (not request or not request->infer_request)
        throw std::invalid_argument("InferRequest is absent");

    InferenceEngine::Blob::Ptr blob = WrapImageToBlob(src_img, *wrap_strategy);
    if (!blob)
        throw std::runtime_error("Could not wrap image");

    if (batch_size > 1) {
        request->blob.push_back(blob);
        if (request->blob.size() >= batch_size) {
            auto blob = InferenceEngine::make_shared_blob<InferenceEngine::BatchedBlob>(request->blob);
            request->infer_request->SetBlob(input_name, blob);
            request->blob.clear();
        }
    } else {
        request->infer_request->SetBlob(input_name, blob);
    }
}

bool OpenVINOImageInference::DoNeedImagePreProcessing() const {
    return pre_processor.get() != nullptr;
}

void OpenVINOImageInference::ApplyInputPreprocessors(
    std::shared_ptr<BatchRequest> &request, const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    ITT_TASK(__FUNCTION__);
    assert(request && "Batch request is null");

    for (const auto &preprocessor : input_preprocessors) {
        if (preprocessor.second == nullptr)
            continue;

        std::string layer_name = (inputs.size() == 1) ? image_layer : preprocessor.second->name;
        if (preprocessor.first == KEY_image) {
            if (!DoNeedImagePreProcessing())
                continue;
        }
        if (inputs.count(layer_name)) {
            InferenceEngine::Blob::Ptr ie_blob = request->infer_request->GetBlob(layer_name);
            InputBlob::Ptr blob = std::make_shared<OpenvinoInputBlob>(ie_blob);
            preprocessor.second->preprocessor(blob);
        } else {
            throw std::invalid_argument("Network does not contain layer: " + layer_name);
        }
    }
}

void OpenVINOImageInference::SubmitImage(
    IFrameBase::Ptr frame, const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) {
    GVA_DEBUG("enter");
    ITT_TASK(__FUNCTION__);

    if (!frame)
        throw std::invalid_argument("Invalid frame provided");

    std::unique_lock<std::mutex> lk(requests_mutex_);
    ++requests_processing_;
    std::shared_ptr<BatchRequest> request = freeRequests.pop();

    try {
        if (DoNeedImagePreProcessing()) {
            SubmitImageProcessing(
                image_layer, request, *frame->GetImage(),
                getImagePreProcInfo(input_preprocessors), // contain operations order for Custom Image PreProcessing
                frame->GetImageTransformationParams() // during CIPP will be filling of crop and aspect-ratio parameters
            );
            // After running this function self-managed image memory appears, and the old image memory can be released
            frame->SetImage(nullptr);
        } else {
            BypassImageProcessing(image_layer, request, *frame->GetImage(), safe_convert<size_t>(batch_size));
        }

        ApplyInputPreprocessors(request, input_preprocessors);

        request->buffers.push_back(frame);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Pre-processing was failed."));
    }

    try {
        // start inference asynchronously if enough buffers for batching
        if (request->buffers.size() >= safe_convert<size_t>(batch_size)) {
            request->infer_request->StartAsync();
        } else {
            freeRequests.push_front(request);
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Inference async start was failed."));
    }
}

const std::string &OpenVINOImageInference::GetModelName() const {
    return model_name;
}

size_t OpenVINOImageInference::GetNireq() const {
    return safe_convert<size_t>(nireq);
}

void OpenVINOImageInference::GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                                    int &memory_type_) const {
    if (inputs.empty())
        throw std::invalid_argument("DL model input layers info is empty");
    auto blob = inputs.find(image_layer);
    if (blob == inputs.end())
        throw std::invalid_argument("Can not get image input blob by name: " + image_layer);

    auto desc = blob->second->getTensorDesc();
    auto dims = desc.getDims();
    auto layout = desc.getLayout();
    switch (layout) {
    case InferenceEngine::Layout::NCHW:
        batch_size = dims[0];
        height = dims[2];
        width = dims[3];
        break;
    case InferenceEngine::Layout::NHWC:
        batch_size = dims[0];
        height = dims[1];
        width = dims[2];
        break;
    default:
        throw std::invalid_argument("Unsupported layout for image");
    }
    switch (memory_type) {
    case MemoryType::SYSTEM:
        format = FourCC::FOURCC_RGBP;
        break;
    case MemoryType::VAAPI:
        format = FourCC::FOURCC_NV12;
        break;
    default:
        throw std::runtime_error("Unsupported memory type");
    }
    memory_type_ = static_cast<int>(memory_type);
}

std::map<std::string, std::vector<size_t>> OpenVINOImageInference::GetModelInputsInfo() const {
    std::map<std::string, std::vector<size_t>> info;
    for (auto input : inputs) {
        info[input.first] = input.second->getTensorDesc().getDims();
    }

    return info;
}

std::map<std::string, std::vector<size_t>> OpenVINOImageInference::GetModelOutputsInfo() const {
    std::map<std::string, std::vector<size_t>> info;
    for (auto output : outputs) {
        info[output.first] = output.second->getTensorDesc().getDims();
    }

    return info;
}

void OpenVINOImageInference::Flush() {
    GVA_DEBUG("enter");
    ITT_TASK(__FUNCTION__);

    // because Flush can execute by several threads for one InferenceImpl instance
    // it must be synchronous.
    std::unique_lock<std::mutex> requests_lk(requests_mutex_);

    std::unique_lock<std::mutex> flush_lk(flush_mutex);

    while (requests_processing_ != 0) {
        auto request = freeRequests.pop();

        if (request->buffers.size() > 0) {
            try {
                // WA: Fill non-complete batch with last element. Can be removed once supported in OV
                if (batch_size > 1 && !DoNeedImagePreProcessing()) {
                    for (int i = request->blob.size(); i < batch_size; i++)
                        request->blob.push_back(request->blob.back());

                    auto blob = InferenceEngine::make_shared_blob<InferenceEngine::BatchedBlob>(request->blob);
                    request->infer_request->SetBlob(image_layer, blob);
                    request->blob.clear();
                }

                request->infer_request->StartAsync();
            } catch (const std::exception &e) {
                GVA_ERROR("Couldn't start inferece on flush: %s", e.what());
                this->handleError(request->buffers);
                FreeRequest(request);
            }
        } else {
            freeRequests.push(request);
        }

        // wait_for unlocks flush_mutex until we get notify
        // waiting will be continued if requests_processing_ != 0
        request_processed_.wait_for(flush_lk, std::chrono::seconds(1), [this] { return requests_processing_ == 0; });
    }
}

void OpenVINOImageInference::Close() {
    Flush();
    while (!freeRequests.empty()) {
        auto req = freeRequests.pop();
        // as earlier set callbacks own shared pointers we need to set lambdas with the empty capture lists
        req->infer_request->SetCompletionCallback([] {});
        if (allocator) {
            for (auto ac : req->alloc_context)
                allocator->Free(ac);
        }
    }
}

void OpenVINOImageInference::WorkingFunction(const std::shared_ptr<BatchRequest> &request) {
    GVA_DEBUG("enter");
    assert(request);

    std::map<std::string, OutputBlob::Ptr> output_blobs;
    for (auto output : outputs) {
        const std::string &name = output.first;
        output_blobs[name] = std::make_shared<OpenvinoOutputBlob>(request->infer_request->GetBlob(name));
    }
    callback(output_blobs, request->buffers);
}
