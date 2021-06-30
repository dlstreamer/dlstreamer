/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "classification_history.h"
#include "common/input_model_preproc.h"
#include "gstgvaclassify.h"
#include "gva_base_inference.h"

#include "inference_backend/image_inference.h"

#include <gst/video/video.h>

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class InferenceImpl {
  public:
    struct Model {
        std::string name;
        std::shared_ptr<InferenceBackend::ImageInference> inference;
        std::vector<ModelInputProcessorInfo::Ptr> input_processor_info;
        std::map<std::string, GstStructure *> output_processor_info;
        std::map<std::string, std::vector<std::string>> labels;
    };

    InferenceImpl(GvaBaseInference *gva_base_inference);

    GstFlowReturn TransformFrameIp(GvaBaseInference *element, GstBuffer *buffer);
    void SinkEvent(GstEvent *event);
    void FlushInference();
    const Model &GetModel() const;

    void UpdateObjectClasses(GvaBaseInference *gva_base_inference);
    bool FilterObjectClass(GstVideoRegionOfInterestMeta *roi) const;
    bool FilterObjectClass(const std::string &object_class) const;

    ~InferenceImpl();

  private:
    InferenceBackend::MemoryType memory_type;
    struct InferenceResult : public InferenceBackend::ImageInference::IFrameBase {
        void SetImage(const std::shared_ptr<InferenceBackend::Image> &image_) override {
            image = image_;
        }
        std::shared_ptr<InferenceFrame> inference_frame;
        Model *model;
        std::shared_ptr<InferenceBackend::Image> image;
    };

    enum InferenceStatus {
        INFERENCE_EXECUTED = 1,
        INFERENCE_SKIPPED_PER_PROPERTY = 2, // frame skipped due to inference-interval set to value greater than 1
        INFERENCE_SKIPPED_NO_BLOCK = 3,     // frame skipped due to no-block policy
        INFERENCE_SKIPPED_ROI = 4           // roi skipped because is_roi_inference_needed() returned false
    };

    std::vector<std::string> object_classes;

    mutable std::mutex _mutex;
    Model model;
    std::shared_ptr<InferenceBackend::Allocator> allocator;

    struct OutputFrame {
        GstBuffer *buffer;
        GstBuffer **writable_buffer;
        uint64_t inference_count;
        GvaBaseInference *filter;
        std::vector<std::shared_ptr<InferenceFrame>> inference_rois;
    };

    std::list<OutputFrame> output_frames;
    std::mutex output_frames_mutex;

    void PushOutput();
    void PushBufferToSrcPad(OutputFrame &output_frame);
    void PushFramesIfInferenceFailed(std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);
    void InferenceCompletionCallback(std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
                                     std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);
    void UpdateOutputFrames(std::shared_ptr<InferenceFrame> &inference_roi);
    Model CreateModel(GvaBaseInference *gva_base_inference, const std::string &model_file,
                      const std::string &model_proc_path);
    void UpdateModelReshapeInfo(GvaBaseInference *gva_base_inference);

    GstFlowReturn SubmitImages(GvaBaseInference *gva_base_inference,
                               const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstBuffer *buffer);
    std::shared_ptr<InferenceResult> MakeInferenceResult(GvaBaseInference *gva_base_inference, Model &model,
                                                         GstVideoRegionOfInterestMeta *meta,
                                                         std::shared_ptr<InferenceBackend::Image> &image,
                                                         GstBuffer *buffer);
};
