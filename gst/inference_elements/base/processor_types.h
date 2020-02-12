/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
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

struct InferenceFrame {
    GstBuffer *buffer;
    GstVideoInfo *info;
    GstVideoRegionOfInterestMeta roi;

    GvaBaseInference *gva_base_inference;
    InferenceFrame() : roi() {
        buffer = nullptr;
        info = nullptr;
        gva_base_inference = nullptr;
    }
    InferenceFrame(GstBuffer *_buff, GstVideoInfo *_info, GstVideoRegionOfInterestMeta _roi, GvaBaseInference *_gbi) {
        buffer = _buff;
        roi = _roi;
        gva_base_inference = _gbi;
        info = (_info != nullptr) ? gst_video_info_copy(_info) : nullptr;
    }
    InferenceFrame(const InferenceFrame &inf) {
        buffer = inf.buffer;
        roi = inf.roi;
        gva_base_inference = inf.gva_base_inference;
        this->info = (inf.info != nullptr) ? gst_video_info_copy(inf.info) : nullptr;
    }
    InferenceFrame &operator=(const InferenceFrame &rhs) {
        buffer = rhs.buffer;
        roi = rhs.roi;
        gva_base_inference = rhs.gva_base_inference;
        if (this->info != nullptr) {
            gst_video_info_free(this->info);
            this->info = nullptr;
        }
        if (rhs.info != nullptr)
            this->info = gst_video_info_copy(rhs.info);

        return *this;
    }
    ~InferenceFrame() {
        if (info != nullptr) {
            gst_video_info_free(info);
            info = nullptr;
        }
    }
};

using RoiPreProcessorFunction = std::function<void(InferenceBackend::Image &)>;

typedef void (*PreProcFunction)(GstStructure *preproc, InferenceBackend::Image &image);

typedef RoiPreProcessorFunction (*GetROIPreProcFunction)(GstStructure *preproc, GstVideoRegionOfInterestMeta *roi_meta);

typedef void (*PostProcFunction)(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                 std::vector<InferenceFrame> frames,
                                 const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name);

typedef bool (*IsROIClassificationNeededFunction)(GvaBaseInference *gva_base_inference, guint current_num_frame,
                                                  GstBuffer *buffer, GstVideoRegionOfInterestMeta *roi);

#else // __cplusplus

typedef void *PreProcFunction;
typedef void *PostProcFunction;
typedef void *GetROIPreProcFunction;
typedef void *IsROIClassificationNeededFunction;

#endif // __cplusplus
