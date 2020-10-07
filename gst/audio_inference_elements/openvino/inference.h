/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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

class OpenVINOAudioInference {
  public:
    OpenVINOAudioInference(const char *model, char *device, AudioInferenceOutput &infOutput);
    virtual ~OpenVINOAudioInference();
    std::vector<uint8_t> convertFloatToU8(std::vector<float> &normalized_samples);
    void setInputBlob(void *buffer_ptr);
    void infer();
    AudioInferenceOutput *getInferenceOutput();

  private:
    InferenceEngine::InferRequest inferRequest;
    std::string model_name;
    InferenceEngine::TensorDesc tensor_desc;
    std::string input_name;
    AudioInferenceOutput infOut;
};