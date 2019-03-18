/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __INFERENCE_H__
#define __INFERENCE_H__

#include <gst/video/video.h>

struct _GstGvaInference;
typedef struct _GstGvaInference GstGvaInference;

typedef struct _InferenceProxy InferenceProxy;

#ifdef __cplusplus

#include "blob2metadata.h"

#include "gva_tensor_meta.h"
#include "inference_backend/image_inference.h"
#include <list>
#include <mutex>

struct InferenceRefs {
    unsigned int numRefs = 0;
    std::list<GstGvaInference *> elementsToInit;
    GstGvaInference *masterElement = nullptr;
    InferenceProxy *proxy = nullptr;
};

class Inference {
  private:
    struct OutputFrame {
        GstBuffer *buffer;
        GstBaseTransform *filter;
    };

    struct InferenceResult : public InferenceBackend::ImageInference::IFrameBase {
        InferenceFrame inference_frame;
        std::pair<int, int> inference_frame_size;
        std::list<OutputFrame> output_frames;
    };

  public:
    Inference(GstGvaInference *ovino);

    static InferenceProxy *aquire_instance(GstGvaInference *ovino);
    static void release_instance(GstGvaInference *ovino);

    void WorkingFunction(GstGvaInference *ovino);
    GstFlowReturn TransformFrameIp(GstGvaInference *ovino, GstBaseTransform *trans, GstBuffer *buffer,
                                   GstVideoInfo *info);
    void SinkEvent(GstEvent *event);
    void FlushInference();

    ~Inference();

  private:
    static void fillElementProps(GstGvaInference *targetElem, GstGvaInference *masterElem);
    static void initExistingElements(InferenceRefs *infRefs);

    void InferenceCompletionCallback(std::map<std::string, InferenceBackend::OutputBlob::Ptr> blobs,
                                     std::vector<std::shared_ptr<InferenceBackend::ImageInference::IFrameBase>> frames);

    mutable std::mutex _mutex;
    int frame_num;
    std::shared_ptr<InferenceBackend::ImageInference> image_inference;
    std::shared_ptr<InferenceResult> result;
    static std::map<std::string, InferenceRefs *> inference_pool_;
    static std::mutex inference_pool_mutex_;
    GstStructure *post_proc;
    GstGvaInference *ovino_;
};
#else /* __cplusplus */

typedef struct Inference Inference;

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _InferenceProxy {
    Inference *instance;
};

InferenceProxy *aquire_inference(GstGvaInference *ovino, GError **error);
void release_inference(GstGvaInference *ovino);
void inference_sink_event(GstGvaInference *ovino, GstEvent *event);
GstFlowReturn frame_to_inference(GstGvaInference *ovino, GstBaseTransform *trans, GstBuffer *buf, GstVideoInfo *info);
void flush_inference(GstGvaInference *ovino);

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */

#endif /* __INFERENCE_H__ */
