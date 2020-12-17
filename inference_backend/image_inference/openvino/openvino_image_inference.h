/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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
class OpenVINOImageInference : public InferenceBackend::ImageInference {
  public:
    OpenVINOImageInference(const std::string &model,
                           const std::map<std::string, std::map<std::string, std::string>> &config,
                           InferenceBackend::Allocator *allocator, CallbackFunc callback,
                           ErrorHandlingFunc error_handler, InferenceBackend::MemoryType memory_type);

    OpenVINOImageInference(const std::string &model,
                           const std::map<std::string, std::map<std::string, std::string>> &config, void *display,
                           CallbackFunc callback, ErrorHandlingFunc error_handler,
                           InferenceBackend::MemoryType memory_type);

    void CreateInferRequests();

    virtual ~OpenVINOImageInference();

    virtual void Init() override;

    virtual void
    SubmitImage(const InferenceBackend::Image &image, IFrameBase::Ptr user_data,
                const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) override;

    virtual const std::string &GetModelName() const override;

    virtual void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                        int &memory_type) const override;

    virtual bool IsQueueFull() override;

    virtual void Flush() override;

    virtual void Close() override;

  protected:
    bool initialized;

    struct BatchRequest {
        InferenceEngine::InferRequest::Ptr infer_request;
        std::vector<IFrameBase::Ptr> buffers;
        std::vector<InferenceBackend::Allocator::AllocContext *> alloc_context;
        InferenceEngine::RemoteContext::Ptr ie_remote_context;
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
    InferenceEngine::Core core;
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

    // Threading
    std::mutex requests_mutex_;
    std::atomic<unsigned int> requests_processing_;
    std::condition_variable request_processed_;
    std::mutex flush_mutex;

    std::queue<InferenceBackend::OutputBlob> output_blob_pool;

  private:
    bool doNeedImagePreProcessing();
    void SubmitImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                               const InferenceBackend::Image &src_img,
                               const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info,
                               const InferenceBackend::ImageTransformationParams::Ptr image_transform_info);
    void BypassImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                               const InferenceBackend::Image &src_img);
    void setCompletionCallback(std::shared_ptr<BatchRequest> &batch_request);
    void
    ApplyInputPreprocessors(std::shared_ptr<BatchRequest> &request,
                            const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors);
    void setBlobsToInferenceRequest(const std::map<std::string, InferenceEngine::TensorDesc> &layers,
                                    std::shared_ptr<BatchRequest> &batch_request,
                                    InferenceBackend::Allocator *allocator);
};
