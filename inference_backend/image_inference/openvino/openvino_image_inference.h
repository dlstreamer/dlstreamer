/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"

#include <atomic>
#include <inference_engine.hpp>
#include <map>
#include <string>
#include <thread>

#include "config.h"
#include "safe_queue.h"

class OpenVINOImageInference : public InferenceBackend::ImageInference {
  public:
    OpenVINOImageInference(const std::string &model,
                           const std::map<std::string, std::map<std::string, std::string>> &config,
                           InferenceBackend::Allocator *allocator, CallbackFunc callback,
                           ErrorHandlingFunc error_handler);

    void CreateInferRequests();

    virtual ~OpenVINOImageInference();

    virtual void
    SubmitImage(const InferenceBackend::Image &image, IFramePtr user_data,
                const std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> &input_preprocessors) override;

    virtual const std::string &GetModelName() const;

    virtual void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format) const;

    virtual bool IsQueueFull();

    virtual void Flush();

    virtual void Close();

  protected:
    bool initialized;

    struct BatchRequest {
        InferenceEngine::InferRequest::Ptr infer_request;
        std::vector<IFramePtr> buffers;
        std::vector<InferenceBackend::Allocator::AllocContext *> alloc_context;
    };

    // InferenceBackend::Image GetNextImageBuffer(std::shared_ptr<BatchRequest> request);
    void HandleError(const std::shared_ptr<BatchRequest> &request);
    void WorkingFunction(const std::shared_ptr<BatchRequest> &request);

    InferenceBackend::Allocator *allocator;
    CallbackFunc callback;
    ErrorHandlingFunc handleError;

    // Inference Engine
    InferenceEngine::Core core;
    InferenceEngine::ConstInputsDataMap inputs;
    InferenceEngine::ConstOutputsDataMap outputs;
    std::string model_name;
    std::string image_layer;

    // Threading
    const int batch_size;
    SafeQueue<std::shared_ptr<BatchRequest>> freeRequests;

    std::unique_ptr<InferenceBackend::PreProc> pre_processor;

    std::mutex mutex_;
    std::atomic<unsigned int> requests_processing_;
    std::condition_variable request_processed_;
    std::mutex flush_mutex;

    std::queue<InferenceBackend::OutputBlob> output_blob_pool;

  private:
    void SubmitImageProcessing(const std::string &input_name, std::shared_ptr<BatchRequest> request,
                               const InferenceBackend::Image &src_img);
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
