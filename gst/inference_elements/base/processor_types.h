/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef __cplusplus

#include "common/input_model_preproc.h"
#include "inference_backend/image_inference.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <functional>

struct _GvaBaseInference;
typedef struct _GvaBaseInference GvaBaseInference;

struct InferenceFrame {
    GstBuffer *buffer;
    GstVideoRegionOfInterestMeta roi;
    std::vector<GstStructure *> roi_classifications; // length equals to output layers count
    GvaBaseInference *gva_base_inference;
    GstVideoInfo *info;

    InferenceFrame() = default;
    InferenceFrame(GstBuffer *_buf, GstVideoRegionOfInterestMeta _roi, std::vector<GstStructure *> _roi_classifications,
                   GvaBaseInference *_gva_base_inference, GstVideoInfo *_info)
        : buffer(_buf), roi(_roi), roi_classifications(_roi_classifications), gva_base_inference(_gva_base_inference) {
        info = (_info) ? gst_video_info_copy(_info) : nullptr;
    }
    InferenceFrame(const InferenceFrame &inf)
        : buffer(inf.buffer), roi(inf.roi), roi_classifications(inf.roi_classifications),
          gva_base_inference(inf.gva_base_inference) {
        this->info = (inf.info) ? gst_video_info_copy(inf.info) : nullptr;
    }
    InferenceFrame &operator=(const InferenceFrame &rhs) {
        buffer = rhs.buffer;
        roi = rhs.roi;
        roi_classifications = rhs.roi_classifications;
        gva_base_inference = rhs.gva_base_inference;
        if (this->info) {
            gst_video_info_free(this->info);
            this->info = nullptr;
        }
        if (rhs.info)
            this->info = gst_video_info_copy(rhs.info);

        return *this;
    }
    ~InferenceFrame() {
        if (info) {
            gst_video_info_free(info);
            info = nullptr;
        }
    }
};

using InputPreprocessingFunction = std::function<void(const InferenceBackend::InputBlob::Ptr &)>;

typedef InputPreprocessingFunction (*InputPreprocessingFunctionGetter)(
    const std::shared_ptr<InferenceBackend::ImageInference> &inference, GstStructure *preproc,
    GstVideoRegionOfInterestMeta *roi_meta);
typedef std::map<std::string, InferenceBackend::InputLayerDesc::Ptr> (*InputPreprocessorsFactory)(
    const std::shared_ptr<InferenceBackend::ImageInference> &inference,
    const std::vector<ModelInputProcessorInfo::Ptr> &model_input_processor_info, GstVideoRegionOfInterestMeta *roi);
typedef void (*PreProcFunction)(GstStructure *preproc, InferenceBackend::Image &image);
typedef bool (*IsROIClassificationNeededFunction)(GvaBaseInference *gva_base_inference, guint64 current_num_frame,
                                                  GstBuffer *buffer, GstVideoRegionOfInterestMeta *roi);

#include "../common/post_processor.h"
using PostProcessorExitStatus = PostProcessor::ExitStatus;

#else // __cplusplus

typedef void *PreProcFunction;
typedef void *InputPreprocessorsFactory;
typedef void *InputPreprocessingFunctionGetter;
typedef struct PostProcessor PostProcessor;
typedef struct PostProcessorExitStatus PostProcessorExitStatus;
typedef void *IsROIClassificationNeededFunction;

#endif // __cplusplus
