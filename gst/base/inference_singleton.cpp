/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_singleton.h"

#include "gva_base_inference.h"
#include "inference_impl.h"
#include <assert.h>

struct InferenceRefs {
    unsigned int numRefs = 0;
    std::list<GvaBaseInference *> elementsToInit;
    GvaBaseInference *masterElement = nullptr;
    InferenceImpl *proxy = nullptr;
};

static std::map<std::string, InferenceRefs *> inference_pool_;
static std::mutex inference_pool_mutex_;

#define COPY_GSTRING(_DST, _SRC)                                                                                       \
    g_free(_DST);                                                                                                      \
    _DST = g_strdup(_SRC);

void fillElementProps(GvaBaseInference *targetElem, GvaBaseInference *masterElem, InferenceImpl *inference_impl) {
    assert(masterElem);
    targetElem->inference = inference_impl;

    COPY_GSTRING(targetElem->model, masterElem->model);
    COPY_GSTRING(targetElem->object_class, masterElem->object_class);
    COPY_GSTRING(targetElem->device, masterElem->device);
    COPY_GSTRING(targetElem->model_proc, masterElem->model_proc);
    targetElem->batch_size = masterElem->batch_size;
    targetElem->every_nth_frame = masterElem->every_nth_frame;
    targetElem->nireq = masterElem->nireq;
    COPY_GSTRING(targetElem->cpu_streams, masterElem->cpu_streams);
    COPY_GSTRING(targetElem->infer_config, masterElem->infer_config);
    COPY_GSTRING(targetElem->allocator_name, masterElem->allocator_name);
    // no need to copy inference_id because it should match already.
}

void initExistingElements(InferenceRefs *infRefs) {
    assert(infRefs->masterElement);
    for (auto elem : infRefs->elementsToInit) {
        fillElementProps(elem, infRefs->masterElement, infRefs->proxy);
    }
}

InferenceImpl *acquire_inference_instance(GvaBaseInference *ovino, GError **error) {
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name(ovino->inference_id);

        InferenceRefs *infRefs = nullptr;
        auto it = inference_pool_.find(name);
        if (it == inference_pool_.end()) {
            infRefs = new InferenceRefs;
            infRefs->numRefs++;
            if (ovino->model) {
                infRefs->proxy = new InferenceImpl(ovino);

                // save master element for later reference from slave elements
                infRefs->masterElement = ovino;
            } else {
                // lazy initialization
                infRefs->proxy = nullptr;
                infRefs->elementsToInit.push_back(ovino);
            }
            inference_pool_.insert({name, infRefs}); // CHECKME will pair has ref to string or string copy?
        } else {
            infRefs = it->second;

            // always increment ref counter
            infRefs->numRefs++;

            // if model arg is passed and there is no instance yet, we create it and make available to all instances
            if (!infRefs->proxy && ovino->model) {
                infRefs->proxy = new InferenceImpl(ovino);

                // save master element for later reference from slave elements
                infRefs->masterElement = ovino;

                // iterate through already created elements and set their props
                initExistingElements(infRefs);
                infRefs->elementsToInit.clear();

            } else if (infRefs->proxy && !ovino->model) {
                // TODO add warning saying that specified props will be ignored if specified for slave.
                // Setting missing props to current elem
                fillElementProps(ovino, infRefs->masterElement, infRefs->proxy);
            } else if (infRefs->proxy && ovino->model) {
                infRefs->elementsToInit.push_back(ovino);
                GST_WARNING_OBJECT(ovino,
                                   "Only one element for each inference-id can have other properties specified.");
                // TODO implement comprehensive validation if all options match btw slave and master and if not:
                // throw std::runtime_error("Only one element for each inference-id can have other properties
                // specified.");
            } else {
                infRefs->elementsToInit.push_back(ovino);
            }
        }
        return infRefs->proxy;
    } catch (const std::exception &exc) {
        g_set_error(error, 1, 1, "%s", exc.what());
        return nullptr;
    }
}

void release_inference_instance(GvaBaseInference *ovino) {
    std::lock_guard<std::mutex> guard(inference_pool_mutex_);
    std::string name(ovino->inference_id);

    auto it = inference_pool_.find(name);
    if (it == inference_pool_.end())
        return;

    InferenceRefs *infRefs = it->second;
    auto refcounter = --infRefs->numRefs;
    if (refcounter == 0) {
        delete infRefs->proxy;
        delete infRefs;
        inference_pool_.erase(name);
    }
}

GstFlowReturn frame_to_classify_inference(GvaBaseInference *ovino, GstBaseTransform *trans, GstBuffer *buf,
                                          GstVideoInfo *info) {
    if (!ovino || !ovino->inference) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    return ((InferenceImpl *)ovino->inference)->TransformFrameIp(ovino, trans, buf, info);
}

void classify_inference_sink_event(GvaBaseInference *ovino, GstEvent *event) {
    ((InferenceImpl *)ovino->inference)->SinkEvent(event);
}

void flush_inference_classify(GvaBaseInference *ovino) {
    if (!ovino || !ovino->inference) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return;
    }

    ((InferenceImpl *)ovino->inference)->FlushInference();
}
