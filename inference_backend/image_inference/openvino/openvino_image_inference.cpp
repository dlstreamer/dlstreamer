/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "openvino_image_inference.h"

#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "inference_backend/safe_arithmetic.h"
#include "model_loader.h"
#include "openvino_blob_wrapper.h"
#include "utils.h"
#include "wrap_image.h"

#include <chrono>
#include <functional>
#include <ie_compound_blob.h>
#include <ie_core.hpp>
#include <inference_engine.hpp>
#include <stdio.h>
#include <thread>

namespace IE = InferenceEngine;
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
        nireq = executable_network.GetMetric(IE::Metrics::METRIC_OPTIMAL_NUMBER_OF_INFER_REQUESTS).as<unsigned int>() +
                1; // One additional for pre-processing parallelization with inference
        std::string msg = "Setting the optimal number of inference requests: nireq=" + std::to_string(nireq);
        GVA_INFO(msg.c_str());
    } catch (const std::exception &e) {
        std::string err = std::string("Failed to get optimal number of inference requests: ") + e.what() +
                          std::string("\nNumber of inference requests will fallback to 1");
        GVA_ERROR(err.c_str());
        return 1;
    }
    return nireq;
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
    }

    return std::string("UNKNOWN_IE_STATUS_CODE");
}

void OpenVINOImageInference::setCompletionCallback(std::shared_ptr<BatchRequest> &batch_request) {
    auto completion_callback = [this, batch_request](InferenceEngine::InferRequest, InferenceEngine::StatusCode code) {
        try {
            ITT_TASK("completion_callback_lambda");
            size_t buffer_size = batch_request->buffers.size();

            if (code != InferenceEngine::StatusCode::OK) {
                std::string msg = "Inference request completion callback failed with InferenceEngine::StatusCode: " +
                                  std::to_string(code) + "\n\t";
                msg += getErrorMsg(code);
                GVA_ERROR(msg.c_str());
                this->handleError(batch_request->buffers);
            } else {
                this->WorkingFunction(batch_request);
            }

            batch_request->buffers.clear();
            this->freeRequests.push(batch_request);
            this->requests_processing_ -= buffer_size;
            this->request_processed_.notify_all();
        } catch (const std::exception &e) {
            std::string msg = "Failed in inference request completion callback:\n" + Utils::createNestedErrorMsg(e);
            GVA_ERROR(msg.c_str());
        }
    };
    batch_request->infer_request
        ->SetCompletionCallback<std::function<void(InferenceEngine::InferRequest, InferenceEngine::StatusCode)>>(
            completion_callback);
}

void OpenVINOImageInference::setBlobsToInferenceRequest(
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

OpenVINOImageInference::OpenVINOImageInference(const std::string &model,
                                               const std::map<std::string, std::map<std::string, std::string>> &config,
                                               Allocator *allocator, CallbackFunc callback,
                                               ErrorHandlingFunc error_handler, MemoryType memory_type)
    : initialized(), allocator(allocator), memory_type(memory_type),
      batch_size(std::stoi(config.at(KEY_BASE).at(KEY_BATCH_SIZE))), requests_processing_(0U) {
    builder = ModelLoader::is_compile_model(model) ? std::unique_ptr<EntityBuilder>(new CompiledBuilder(config, model))
                                                   : std::unique_ptr<EntityBuilder>(new IrBuilder(config, model));
    if (not builder)
        throw std::runtime_error("Failed to create DL model loader");
    network = builder->createNetwork(core);
    model_name = builder->getNetworkName(network);
    this->callback = callback;
    this->handleError = error_handler;
    const std::map<std::string, std::string> &base_config = config.at(KEY_BASE);
    nireq = std::stoi(base_config.at(KEY_NIREQ));
    this->display = nullptr;
}

OpenVINOImageInference::OpenVINOImageInference(const std::string &model,
                                               const std::map<std::string, std::map<std::string, std::string>> &config,
                                               void *display, CallbackFunc callback, ErrorHandlingFunc error_handler,
                                               MemoryType memory_type)
    : initialized(), display(display), memory_type(memory_type),
      batch_size(std::stoi(config.at(KEY_BASE).at(KEY_BATCH_SIZE))), requests_processing_(0U) {
    builder = ModelLoader::is_compile_model(model)
                  ? std::unique_ptr<EntityBuilder>(new CompiledBuilder(config, model))
                  : std::unique_ptr<EntityBuilder>(new IrBuilder(config, model, display));
    if (not builder)
        throw std::runtime_error("Failed to create DL model loader");
    network = builder->createNetwork(core);
    model_name = builder->getNetworkName(network);
    this->callback = callback;
    this->handleError = error_handler;
    const std::map<std::string, std::string> &base_config = config.at(KEY_BASE);
    nireq = std::stoi(base_config.at(KEY_NIREQ));
    this->allocator = nullptr;
}

void OpenVINOImageInference::Init() {
    try {
        InferenceEngine::ExecutableNetwork executable_network;
        std::tie(pre_processor, executable_network, image_layer) =
            builder->createPreProcAndExecutableNetwork(network, core);

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
            nireq = optimalNireq(executable_network);
        }

        for (int i = 0; i < nireq; i++) {
            std::shared_ptr<BatchRequest> batch_request = std::make_shared<BatchRequest>();
            batch_request->infer_request = executable_network.CreateInferRequestPtr();
            if (memory_type == MemoryType::DMA_BUFFER or memory_type == MemoryType::VAAPI)
                batch_request->ie_remote_context = executable_network.GetContext();
            setCompletionCallback(batch_request);
            if (allocator) {
                setBlobsToInferenceRequest(layers, batch_request, allocator);
            }
            freeRequests.push(batch_request);
        }

        initialized = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to construct OpenVINOImageInference"));
    }
}

bool OpenVINOImageInference::IsQueueFull() {
    return freeRequests.empty();
}

namespace {
template <class T>
Image FillImage(const IE::Blob::Ptr &blob, const IE::SizeVector &dims, const size_t index) {
    Image image = Image();
    image.width = dims[3];
    image.height = dims[2];
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

Image MapBlobBufferToImage(IE::Blob::Ptr blob, size_t batch_index) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);
    auto desc = blob->getTensorDesc();
    auto dims = desc.getDims();
    if (desc.getLayout() != IE::Layout::NCHW) {
        throw std::runtime_error("Unsupported layout");
    }
    Image image = Image();
    switch (desc.getPrecision()) {
    case IE::Precision::FP32:
        image = FillImage<float>(blob, desc.getDims(), batch_index);
        image.format = FourCC::FOURCC_RGBP_F32;
        break;
    case IE::Precision::U8:
        image = FillImage<uint8_t>(blob, desc.getDims(), batch_index);
        image.format = FourCC::FOURCC_RGBP;
        break;
    default:
        throw std::runtime_error("Unsupported precision");
        break;
    }
    return image;
}
} // namespace

void OpenVINOImageInference::SubmitImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                                                   const Image &src_img, const InputImageLayerDesc::Ptr &pre_proc_info,
                                                   const ImageTransformationParams::Ptr image_transform_info) {
    ITT_TASK("SubmitImageProcessing");
    if (not request or not request->infer_request)
        throw std::invalid_argument("InferRequest is absent");
    auto blob = request->infer_request->GetBlob(input_name);
    size_t batch_index = request->buffers.size();
    Image dst_img = MapBlobBufferToImage(blob, batch_index);
    if (src_img.planes[0] != dst_img.planes[0]) { // only convert if different buffers
        try {
            pre_processor->Convert(src_img, dst_img, pre_proc_info, image_transform_info);
        } catch (const std::exception &e) {
            std::throw_with_nested(std::runtime_error("Failed while software frame preprocessing"));
        }
    }
}

void OpenVINOImageInference::BypassImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                                                   const Image &src_img) {
    ITT_TASK("BypassImage");
    if (not request or not request->infer_request)
        throw std::invalid_argument("InferRequest is absent");
    InferenceEngine::Blob::Ptr blob;
    switch (memory_type) {
    case MemoryType::SYSTEM:
        blob = WrapImageToBlob(src_img);
        break;
#ifdef ENABLE_VAAPI
    case MemoryType::DMA_BUFFER:
    case MemoryType::VAAPI:
        blob = WrapImageToBlob(src_img, request->ie_remote_context);
        break;
#endif
    default:
        throw std::invalid_argument("Unsupported memory type");
    }

    request->infer_request->SetBlob(input_name, blob);
}

bool OpenVINOImageInference::doNeedImagePreProcessing() {
    return pre_processor.get() != nullptr;
}

void OpenVINOImageInference::ApplyInputPreprocessors(
    std::shared_ptr<BatchRequest> &request, const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) {
    ITT_TASK(__FUNCTION__);
    for (const auto &preprocessor : input_preprocessors) {
        if (preprocessor.second == nullptr)
            continue;

        std::string layer_name = preprocessor.second->name;
        if (preprocessor.first == KEY_image) {
            if (!doNeedImagePreProcessing()) {
                if (preprocessor.second->input_image_preroc_params)
                    if (preprocessor.second->input_image_preroc_params->isDefined())
                        GVA_WARNING(
                            "The \"pre-process-backend\" was chosen that does not involve a custom preprocessing "
                            "algorithm. Check the description of the pre-processor in the model-proc file and the "
                            "element "
                            "\"pre-process-backend\" property.");
                continue;
            }
            if (inputs.size() == 1)
                layer_name = image_layer;
        }
        if (inputs.count(layer_name)) {
            IE::Blob::Ptr ie_blob = request->infer_request->GetBlob(layer_name);
            InputBlob::Ptr blob = std::make_shared<OpenvinoInputBlob>(ie_blob);
            preprocessor.second->preprocessor(blob);
        } else {
            throw std::invalid_argument("Network does not contain layer: " + layer_name);
        }
    }
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

const InferenceBackend::ImageTransformationParams::Ptr
getImageTransformationParams(OpenVINOImageInference::IFrameBase::Ptr user_data) {
    if (user_data)
        return user_data->GetImageTransformationParams();
    return nullptr;
}

void OpenVINOImageInference::SubmitImage(
    const Image &image, IFrameBase::Ptr user_data,
    const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    if (not initialized) {
        Init();
    }

    std::unique_lock<std::mutex> lk(requests_mutex_);
    ++requests_processing_;
    auto request = freeRequests.pop();

    if (doNeedImagePreProcessing()) {
        SubmitImageProcessing(
            image_layer, request, image,
            getImagePreProcInfo(input_preprocessors), // contain operations order for Custom Image PreProcessing
            getImageTransformationParams(user_data)   // during CIPP will be filling of crop and aspect-ratio parameters
        );
        // After running this function self-managed image memory appears, and the old image memory can be released
        user_data->SetImage(nullptr);
    } else {
        BypassImageProcessing(image_layer, request, image);
    }

    ApplyInputPreprocessors(request, input_preprocessors);

    request->buffers.push_back(user_data);

    // start inference asynchronously if enough buffers for batching
    if (request->buffers.size() >= (size_t)batch_size) {
        request->infer_request->StartAsync();
    } else {
        freeRequests.push_front(request);
    }
}

const std::string &OpenVINOImageInference::GetModelName() const {
    return model_name;
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

void OpenVINOImageInference::Flush() {
    GVA_DEBUG(__FUNCTION__);
    ITT_TASK(__FUNCTION__);

    // because Flush can execute by several threads for one InferenceImpl instance
    // it must be synchronous.
    std::unique_lock<std::mutex> requests_lk(requests_mutex_);

    std::unique_lock<std::mutex> flush_lk(flush_mutex);

    while (requests_processing_ != 0) {
        auto request = freeRequests.pop();
        if (request->buffers.size() > 0) {
            request->infer_request->StartAsync();
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
    GVA_DEBUG(__FUNCTION__);
    std::map<std::string, OutputBlob::Ptr> output_blobs;
    for (auto output : outputs) {
        const std::string &name = output.first;
        output_blobs[name] = std::make_shared<OpenvinoOutputBlob>(request->infer_request->GetBlob(name));
    }
    callback(output_blobs, request->buffers);
}
