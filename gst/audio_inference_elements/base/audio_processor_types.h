/*******************************************************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef __cplusplus

#include "gva_audio_event_meta.h"
#include "inference_backend/image_inference.h"
#include "utils.h"
#include <algorithm>
#include <functional>
#include <gst/gst.h>
#include <map>
#include <string>
#include <vector>

#ifndef __linux__

#ifndef uint
#define uint unsigned int
#endif // uint

#ifndef ulong
#define ulong unsigned long
#endif // ulong

#endif // __linux__

struct _GvaAudioBaseInference;
typedef struct _GvaAudioBaseInference GvaAudioBaseInference;
struct AudioInferenceFrame {
    GstBuffer *buffer;
    std::vector<float> samples;
    gulong startTime;
    gulong endTime;
};

struct AudioInferenceOutput {
    std::string model_name;
    std::map<std::string, std::map<uint, std::pair<std::string, float>>> model_proc;
    std::map<std::string, std::pair<InferenceBackend::OutputBlob::Ptr, int>> output_blobs;
};
typedef int (*AudioNumOfSamplesRequired)(GvaAudioBaseInference *audio_base_inference);
typedef std::vector<float> (*AudioPreProcFunction)(AudioInferenceFrame *frame);
typedef void (*AudioPostProcFunction)(AudioInferenceFrame *frame, AudioInferenceOutput *output);

#else // __cplusplus

typedef void *AudioPreProcFunction;
typedef void *AudioPostProcFunction;
typedef void *AudioNumOfSamplesRequired;

#endif // __cplusplus