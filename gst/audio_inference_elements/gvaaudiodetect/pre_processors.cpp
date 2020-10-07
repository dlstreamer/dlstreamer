/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "pre_processors.h"
#include "gstgvaaudiodetect.h"
#include <functional>
#include <iostream>
#include <limits.h>
#include <math.h>
#include <numeric>
#include <vector>
std::vector<float> GetNormalizedSamples(AudioInferenceFrame *frame) {

    if (!frame || frame->samples.size() == 0)
        throw std::runtime_error("Invalid AudioInferenceFrame object");

    float mean = accumulate(frame->samples.begin(), frame->samples.end(), 0) / (float)frame->samples.size();
    float sq_sum = std::inner_product(frame->samples.begin(), frame->samples.end(), frame->samples.begin(), 0.0);
    float stdev = std::sqrt((sq_sum / (float)frame->samples.size()) - (mean * mean));
    std::vector<float> normalized_samples(frame->samples.size());
    transform(frame->samples.begin(), frame->samples.end(), normalized_samples.begin(),
              [mean, stdev](float v) { return ((v - mean) / (stdev + 1e-15)); });
    return normalized_samples;
}

int GetNumberOfSamplesRequired(GvaAudioBaseInference *audio_base_inference) {
    GvaAudioDetect *gvaaudiodetect = (GvaAudioDetect *)audio_base_inference;
    return gvaaudiodetect->req_num_samples;
}
AudioPreProcFunction GET_NORMALIZED_SAMPLES = GetNormalizedSamples;
AudioNumOfSamplesRequired GET_NUM_OF_SAMPLES_REQUIRED = GetNumberOfSamplesRequired;