/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __CLASSIFY_INFERENCE_H__
#define __CLASSIFY_INFERENCE_H__

#include "gva_base_inference.h"
#include "inference_backend/image_inference.h"

#include <gst/video/video.h>

#include <list>
#include <memory>
#include <mutex>

class InferenceImpl {
  public:
    InferenceImpl(GvaBaseInference *gva_base_inference);

    typedef std::function<void(InferenceBackend::Image &)> (*PreProcessFuncType)(
        GstStructure *preproc, GstVideoRegionOfInterestMeta *roi_meta);

    GstFlowReturn TransformFrameIp(GvaBaseInference *ovino, GstBaseTransform *trans, GstBuffer *buffer,
                                   GstVideoInfo *info);
    void SinkEvent(GstEvent *event);
    void FlushInference();

    ~InferenceImpl();

  private:
    struct ClassificationModel {
        std::string model_name;
        std::string object_class;
        std::shared_ptr<InferenceBackend::ImageInference> inference;
        std::map<std::string, GstStructure *> model_proc;
        GstStructure *input_preproc;
    };

    void PushOutput();
    void InferenceCompletionCallback(std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
                                     std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);
    ClassificationModel CreateClassificationModel(GvaBaseInference *gva_base_inference,
                                                  std::shared_ptr<InferenceBackend::Allocator> &allocator,
                                                  const std::string &model_file, const std::string &model_proc_path,
                                                  const std::string &object_class);

    GstFlowReturn SubmitImages(const std::vector<GstVideoRegionOfInterestMeta *> &metas, GstVideoInfo *info,
                               GstBuffer *buffer);
    void SubmitImage(ClassificationModel &model, GstVideoRegionOfInterestMeta *meta, InferenceBackend::Image &image,
                     GstBuffer *buffer);

    mutable std::mutex _mutex;
    int frame_num;
    std::vector<ClassificationModel> models;
    GvaBaseInference *gva_base_inference;
    std::shared_ptr<InferenceBackend::Allocator> allocator;

    struct OutputFrame {
        GstBuffer *buffer;
        GstBuffer *writable_buffer;
        int inference_count;
        GstBaseTransform *filter;
    };

    struct InferenceResult : public InferenceBackend::ImageInference::IFrameBase {
        InferenceROI inference_frame;
        ClassificationModel *model;
    };

    std::list<OutputFrame> output_frames;
    std::mutex output_frames_mutex;
};

#endif /* __CLASSIFY_INFERENCE_H__ */
