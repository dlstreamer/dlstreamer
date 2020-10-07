/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "gva_audio_base_inference.h"
#include <vector>

class AudioInferImpl {
  public:
    AudioInferImpl(GvaAudioBaseInference *audio_base_inference);
    virtual ~AudioInferImpl();
    void fillAudioFrame(AudioInferenceFrame *frame);
    bool readyToInfer();
    void addSamples(int16_t *samples, uint num_samples, ulong start_time);
    void setNumOfSamplesToSlide();

  private:
    std::vector<float> audioData;
    std::vector<ulong> inferenceStartTime;
    bool startTimeSet = false;
    GvaAudioBaseInference *audio_base_inference;
    void setStartTime(ulong start_time);
    uint sliding_samples = 0;
};