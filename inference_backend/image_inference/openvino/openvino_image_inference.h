/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"
#include "inference_backend/input_image_layer_descriptor.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "model_builder.h"

#include <atomic>
#include <inference_engine.hpp>
#include <map>
#include <string>
#include <thread>

#include "config.h"
#include "safe_queue.h"

struct EntityBuilder;
namespace WrapImageStrategy {
struct General;
}

class OpenVINOImageInference : public InferenceBackend::ImageInference {
  public:
    OpenVINOImageInference(const InferenceBackend::InferenceConfig &config, InferenceBackend::Allocator *allocator,
                           void *display, CallbackFunc callback, ErrorHandlingFunc error_handler,
                           InferenceBackend::MemoryType memory_type);

    virtual ~OpenVINOImageInference();

    virtual void
    SubmitImage(const InferenceBackend::Image &image, IFrameBase::Ptr user_data,
                const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) override;

    virtual const std::string &GetModelName() const override;

    virtual size_t GetNireq() const override;

    virtual void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                        int &memory_type) const override;

    virtual std::map<std::string, std::vector<size_t>> GetModelInputsInfo() const override;
    virtual std::map<std::string, std::vector<size_t>> GetModelOutputsInfo() const override;

    virtual bool IsQueueFull() override;

    virtual void Flush() override;

    virtual void Close() override;

  protected:
    struct BatchRequest {
        InferenceEngine::InferRequest::Ptr infer_request;
        std::vector<IFrameBase::Ptr> buffers;
        std::vector<InferenceBackend::Allocator::AllocContext *> alloc_context;
        std::vector<InferenceEngine::Blob::Ptr> blob;
    };

    // InferenceBackend::Image GetNextImageBuffer(std::shared_ptr<BatchRequest> request);
    void HandleError(const std::shared_ptr<BatchRequest> &request);
    void WorkingFunction(const std::shared_ptr<BatchRequest> &request);

    InferenceBackend::Allocator *allocator;
    void *display;
    InferenceBackend::MemoryType memory_type;
    CallbackFunc callback;
    ErrorHandlingFunc handleError;

    // Inference Engine
    InferenceEngine::ConstInputsDataMap inputs;
    InferenceEngine::ConstOutputsDataMap outputs;
    std::string model_name;
    std::string image_layer;

    const int batch_size;
    int nireq;
    SafeQueue<std::shared_ptr<BatchRequest>> freeRequests;

    std::unique_ptr<EntityBuilder> builder;
    InferenceEngine::CNNNetwork network;
    std::unique_ptr<InferenceBackend::ImagePreprocessor> pre_processor;
    std::unique_ptr<WrapImageStrategy::General> wrap_strategy;

    // Threading
    std::mutex requests_mutex_;
    std::atomic<unsigned int> requests_processing_;
    std::condition_variable request_processed_;
    std::mutex flush_mutex;

  private:
    void FreeRequest(std::shared_ptr<BatchRequest> request);
    InferenceEngine::RemoteContext::Ptr CreateRemoteContext(const InferenceBackend::InferenceConfig &config);
    bool DoNeedImagePreProcessing() const;
    void SubmitImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                               const InferenceBackend::Image &src_img,
                               const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info,
                               const InferenceBackend::ImageTransformationParams::Ptr image_transform_info);
    void BypassImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                               const InferenceBackend::Image &src_img, size_t batch_size);
    void SetCompletionCallback(std::shared_ptr<BatchRequest> &batch_request);
    void
    ApplyInputPreprocessors(std::shared_ptr<BatchRequest> &request,
                            const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors);
    void SetBlobsToInferenceRequest(const std::map<std::string, InferenceEngine::TensorDesc> &layers,
                                    std::shared_ptr<BatchRequest> &batch_request,
                                    InferenceBackend::Allocator *allocator);
    std::unique_ptr<WrapImageStrategy::General>
    CreateWrapImageStrategy(InferenceBackend::MemoryType memory_type, const std::string &device,
                            const InferenceEngine::RemoteContext::Ptr &remote_context);
};
