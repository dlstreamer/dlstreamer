/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "audio_processor_types.h"
#include "inference_backend/image_inference.h"

#include <inference_engine.hpp>
#include <math.h>
#include <memory>
#include <string>
#include <vector>

#ifdef ENABLE_VPUX
#include <ie_remote_context.hpp>
#endif

class OpenVINOAudioInference {
  public:
    OpenVINOAudioInference(const char *model, char *device, AudioInferenceOutput &infOutput);
    virtual ~OpenVINOAudioInference();
    std::vector<uint8_t> convertFloatToU8(std::vector<float> &normalized_samples);
    void setInputBlob(void *buffer_ptr, int dma_fd = 0);
    void infer();
    AudioInferenceOutput *getInferenceOutput();

  private:
    void CreateRemoteContext(const std::string &device);

    InferenceEngine::InferRequest inferRequest;
    InferenceEngine::RemoteContext::Ptr remote_context;
    std::string model_name;
    InferenceEngine::TensorDesc tensor_desc;
    std::string input_name;
    AudioInferenceOutput infOut;
};
