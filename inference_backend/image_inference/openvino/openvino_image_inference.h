/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"
#include "inference_backend/pre_proc.h"

#include <inference_engine.hpp>
#include <map>
#include <string>
#include <thread>

#include "config.h"

#include "inference_backend/logger.h"
#include "safe_queue.h"

using namespace InferenceBackend;

class OpenVINOImageInference : public ImageInference {
  public:
    OpenVINOImageInference(std::string devices, std::string model, int batch_size, int nireq,
                           const std::map<std::string, std::string> &config, CallbackFunc callback);

    virtual ~OpenVINOImageInference();
    virtual void SubmitImage(const Image &image, IFramePtr user_data, std::function<void(Image &)> preProcessor);

    virtual const std::string &GetModelName() const;

    virtual const std::string &GetLayerTypeByLayerName(const std::string &layer_name) const;

    virtual bool IsQueueFull();

    virtual void Flush();

    virtual void Close();

  protected:
    typedef struct {
        InferenceEngine::InferRequest::Ptr infer_request;
        std::vector<IFramePtr> buffers;
    } BatchRequest;

    void GetNextImageBuffer(BatchRequest &request, Image *image);

    void WorkingFunction();

    bool resize_by_inference;
    CallbackFunc callback;

    // Inference Engine
    std::vector<InferenceEngine::InferencePlugin::Ptr> plugins;
    InferenceEngine::ConstInputsDataMap inputs;
    std::map<std::string, std::string> layerNameToType;
    std::string modelName;

    // Threading
    int batch_size;
    std::thread working_thread;
    SafeQueue<BatchRequest> freeRequests;
    SafeQueue<BatchRequest> workingRequests;

    // VPP
    std::unique_ptr<PreProc> sw_vpp;
    std::unique_ptr<PreProc> vaapi_vpp;
    bool already_flushed;
    std::mutex flush_mutex;
};
