/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef __cplusplus

#include "inference_backend/image_inference.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <functional>

struct _GvaBaseInference;
typedef struct _GvaBaseInference GvaBaseInference;

typedef struct {
    GstBuffer *buffer;
    GstVideoRegionOfInterestMeta roi;
    GvaBaseInference *gva_base_inference;
} InferenceROI;

using RoiPreProcessorFunction = std::function<void(InferenceBackend::Image &)>;

typedef void (*PreProcFunction)(GstStructure *preproc, InferenceBackend::Image &image);

typedef RoiPreProcessorFunction (*GetROIPreProcFunction)(GstStructure *preproc, GstVideoRegionOfInterestMeta *roi_meta);

typedef void (*PostProcFunction)(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                 std::vector<InferenceROI> frames,
                                 const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name);

typedef bool (*IsROIClassificationNeededFunction)(GvaBaseInference *gva_base_inference, guint current_num_frame,
                                                  GstBuffer *buffer, GstVideoRegionOfInterestMeta *roi);

#else // __cplusplus

typedef void *PreProcFunction;
typedef void *PostProcFunction;
typedef void *GetROIPreProcFunction;
typedef void *IsROIClassificationNeededFunction;

#endif // __cplusplus
