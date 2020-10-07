/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "audio_infer_impl.h"
#include <cmath>

AudioInferImpl::~AudioInferImpl() {
}

AudioInferImpl::AudioInferImpl(GvaAudioBaseInference *audio_base_inference) {
    this->audio_base_inference = audio_base_inference;
    setNumOfSamplesToSlide();
}

void AudioInferImpl::addSamples(int16_t *samples, uint num_samples, ulong start_time) {
    if (samples && num_samples > 0) {
        setStartTime(start_time);
        audioData.insert(audioData.end(), &samples[0], &samples[num_samples]);
    } else
        throw std::runtime_error("Invalid Input data");
}

bool AudioInferImpl::readyToInfer() {
    return (audioData.size() == audio_base_inference->sample_length);
}

void AudioInferImpl::fillAudioFrame(AudioInferenceFrame *frame) {
    frame->samples = audioData;
    frame->startTime = inferenceStartTime[0];
    frame->endTime = inferenceStartTime[0] + (audioData.size() * MULTIPLIER);
    if (sliding_samples < audio_base_inference->sample_length) {
        audioData.erase(audioData.begin(), audioData.begin() + sliding_samples);
        inferenceStartTime.erase(inferenceStartTime.begin());
    } else {
        audioData.clear();
        inferenceStartTime.clear();
    }
    startTimeSet = false;
}

void AudioInferImpl::setStartTime(ulong start_time) {
    if (sliding_samples < audio_base_inference->sample_length && ((audioData.size() % sliding_samples) == 0)) {
        startTimeSet = false;
    }
    if (!startTimeSet) {
        inferenceStartTime.push_back(start_time);
        startTimeSet = true;
    }
}

void AudioInferImpl::setNumOfSamplesToSlide() {
    sliding_samples = std::round(audio_base_inference->sliding_length * SAMPLE_AUDIO_RATE);
}