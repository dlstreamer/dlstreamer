/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"
#include "inference_backend/pre_proc.h"

#include <atomic>
#include <inference_engine.hpp>
#include <map>
#include <string>
#include <thread>

#include "config.h"

#include "inference_backend/logger.h"
#include "safe_queue.h"

class OpenVINOImageInference : public InferenceBackend::ImageInference {
  public:
    OpenVINOImageInference(std::string devices, std::string model, int batch_size, int nireq,
                           const std::map<std::string, std::string> &config, InferenceBackend::Allocator *allocator,
                           CallbackFunc callback);

    void CreateInferRequests();

    virtual ~OpenVINOImageInference();

    virtual void SubmitImage(const InferenceBackend::Image &image, IFramePtr user_data,
                             std::function<void(InferenceBackend::Image &)> preProcessor);

    virtual const std::string &GetModelName() const;
    virtual void GetModelInputInfo(int *width, int *height, int *format) const;

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

    void GetNextImageBuffer(std::shared_ptr<BatchRequest> request, InferenceBackend::Image *image);

    void WorkingFunction(std::shared_ptr<BatchRequest> request);

    InferenceBackend::Allocator *allocator;
    CallbackFunc callback;

    // Inference Engine
    std::vector<InferenceEngine::InferencePlugin::Ptr> plugins;
    InferenceEngine::ConstInputsDataMap inputs;
    InferenceEngine::ConstOutputsDataMap outputs;
    std::string modelName;
    InferenceEngine::CNNNetwork network;

    // Threading
    const int batch_size;
    SafeQueue<std::shared_ptr<BatchRequest>> freeRequests;

    // VPP
    std::unique_ptr<InferenceBackend::PreProc> sw_vpp;

    std::mutex mutex_;
    std::mutex inference_completion_mutex_;
    std::atomic<unsigned int> requests_processing_;
    std::condition_variable request_processed_;
    std::mutex flush_mutex;

  private:
    void SubmitImageSoftwarePreProcess(std::shared_ptr<BatchRequest> request, const InferenceBackend::Image &src,
                                       std::function<void(InferenceBackend::Image &)> preProcessor);
};
