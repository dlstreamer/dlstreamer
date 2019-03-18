/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __CLASSIFY_INFERENCE_H__
#define __CLASSIFY_INFERENCE_H__

#include <gst/video/video.h>

struct _GstGvaClassify;
typedef struct _GstGvaClassify GstGvaClassify;

typedef struct _ClassifyInferenceProxy ClassifyInferenceProxy;

#ifdef __cplusplus

#include "blob2metadata.h"
#include "inference_backend/image_inference.h"
#include <list>
#include <mutex>

struct InferenceRefs {
    unsigned int numRefs = 0;
    std::list<GstGvaClassify *> elementsToInit;
    GstGvaClassify *masterElement = nullptr;
    ClassifyInferenceProxy *proxy = nullptr;
};

class ClassifyInference {
  public:
    ClassifyInference(GstGvaClassify *ovino);

    static ClassifyInferenceProxy *aquire_instance(GstGvaClassify *ovino);
    static void release_instance(GstGvaClassify *ovino);

    GstFlowReturn TransformFrameIp(GstGvaClassify *ovino, GstBaseTransform *trans, GstBuffer *buffer,
                                   GstVideoInfo *info);
    void SinkEvent(GstEvent *event);
    void FlushInference();

    ~ClassifyInference();

  private:
    static void fillElementProps(GstGvaClassify *targetElem, GstGvaClassify *masterElem);
    static void initExistingElements(InferenceRefs *infRefs);
    void PushOutput();
    void InferenceCompletionCallback(std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
                                     std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);
    std::function<void(InferenceBackend::Image &)> InputPreProcess(const InferenceBackend::Image &image,
                                                                   GstVideoRegionOfInterestMeta *roi_meta,
                                                                   GstStructure *preproc);

    struct ClassificationModel {
        std::string model_name;
        std::string object_class;
        std::shared_ptr<InferenceBackend::ImageInference> inference;
        std::map<std::string, GstStructure *> model_proc;
        GstStructure *input_preproc;
    };
    mutable std::mutex _mutex;
    int frame_num;
    std::vector<ClassificationModel> models;
    static std::map<std::string, InferenceRefs *> inference_pool_;
    static std::mutex inference_pool_mutex_;
    std::string inference_id;

    struct OutputFrame {
        GstBuffer *buffer;
        GstBuffer *writable_buffer;
        int inference_count;
        GstBaseTransform *filter;
    };

    struct InferenceResult : public InferenceBackend::ImageInference::IFrameBase {
        InferenceFrame inference_frame;
        ClassificationModel *model;
    };

    std::list<OutputFrame> output_frames;
    std::mutex output_frames_mutex;
};
#else /* __cplusplus */

typedef struct ClassifyInference ClassifyInference;

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _ClassifyInferenceProxy {
    ClassifyInference *instance;
};

ClassifyInferenceProxy *aquire_classify_inference(GstGvaClassify *ovino, GError **error);
void release_classify_inference(GstGvaClassify *ovino);
void classify_inference_sink_event(GstGvaClassify *ovino, GstEvent *event);
GstFlowReturn frame_to_classify_inference(GstGvaClassify *ovino, GstBaseTransform *trans, GstBuffer *buf,
                                          GstVideoInfo *info);
void flush_inference_classify(GstGvaClassify *ovino);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */

#endif /* __CLASSIFY_INFERENCE_H__ */
