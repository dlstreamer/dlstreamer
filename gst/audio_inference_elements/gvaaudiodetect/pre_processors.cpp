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
void *NormalizeSamples(AudioInferenceFrame *frame) {

    if (!frame || frame->samples.size() == 0)
        throw std::runtime_error("Invalid AudioInferenceFrame object");

    float mean = accumulate(frame->samples.begin(), frame->samples.end(), 0) / frame->samples.size();
    float sq_sum = std::inner_product(frame->samples.begin(), frame->samples.end(), frame->samples.begin(), 0.0);
    float stdev = std::sqrt(sq_sum / frame->samples.size() - mean * mean);
    std::vector<float> blob_buffer(frame->samples.size());
    transform(frame->samples.begin(), frame->samples.end(), blob_buffer.begin(),
              [mean, stdev](int16_t v) { return ((v - mean) / (stdev + 1e-15)); });
    return blob_buffer.data();
}

int GetNumberOfSamplesRequired(GvaAudioBaseInference *audio_base_inference) {
    GvaAudioDetect *gvaaudiodetect = (GvaAudioDetect *)audio_base_inference;
    return gvaaudiodetect->req_num_samples;
}
AudioPreProcFunction NORMALIZE_SAMPLES = NormalizeSamples;
AudioNumOfSamplesRequired GET_NUM_OF_SAMPLES_REQUIRED = GetNumberOfSamplesRequired;