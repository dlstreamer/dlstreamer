/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __BASE_INFERENCE_H__
#define __BASE_INFERENCE_H__

#include "gva_base_inference.h"
#include "inference_backend/image_inference.h"

#include <gst/video/video.h>

#include <list>
#include <memory>
#include <mutex>

class InferenceImpl {
  public:
    struct Model {
        std::string name;
        std::shared_ptr<InferenceBackend::ImageInference> inference;
        std::map<std::string, GstStructure *> proc;
        GstStructure *input_preproc;
    };

    InferenceImpl(GvaBaseInference *gva_base_inference);

    GstFlowReturn TransformFrameIp(GvaBaseInference *element, GstBuffer *buffer, GstVideoInfo *info);
    void SinkEvent(GstEvent *event);
    void FlushInference();
    const std::vector<Model> &GetModels() const;

    ~InferenceImpl();

  private:
    struct InferenceResult : public InferenceBackend::ImageInference::IFrameBase {
        InferenceFrame inference_frame;
        Model *model;
        std::shared_ptr<InferenceBackend::Image> image;
    };

    enum InferenceStatus {
        INFERENCE_EXECUTED = 1,
        INFERENCE_SKIPPED_PER_PROPERTY = 2, // frame skipped due to every-nth-frame set to value greater than 1
        INFERENCE_SKIPPED_ADAPTIVE = 3,     // frame skipped due to adaptive-skip policy
        INFERENCE_SKIPPED_ROI = 4           // roi skipped because is_roi_classification_needed() returned false
    };

    void PushOutput();
    void InferenceCompletionCallback(std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
                                     std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);
    Model CreateModel(GvaBaseInference *gva_base_inference, std::shared_ptr<InferenceBackend::Allocator> &allocator,
                      const std::string &model_file, const std::string &model_proc_path);

    GstFlowReturn SubmitImages(GvaBaseInference *gva_base_inference,
                               const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstVideoInfo *info,
                               GstBuffer *buffer);
    std::shared_ptr<InferenceResult> MakeInferenceResult(GvaBaseInference *gva_base_inference, Model &model,
                                                         GstVideoRegionOfInterestMeta *meta,
                                                         std::shared_ptr<InferenceBackend::Image> image,
                                                         GstBuffer *buffer);
    RoiPreProcessorFunction GetPreProcFunction(GvaBaseInference *gva_base_inference, GstStructure *input_preproc,
                                               GstVideoRegionOfInterestMeta *meta);

    mutable std::mutex _mutex;
    int frame_num;
    std::vector<Model> models;
    std::shared_ptr<InferenceBackend::Allocator> allocator;

    struct OutputFrame {
        GstBuffer *buffer;
        GstBuffer *writable_buffer;
        int inference_count;
        GvaBaseInference *filter;
    };

    std::list<OutputFrame> output_frames;
    std::mutex output_frames_mutex;
};

#endif /* __BASE_INFERENCE_H__ */
