/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_singleton.h"

#include "gva_base_inference.h"
#include "gva_utils.h"
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

void registerElement(GvaBaseInference *ovino, GError **error) {
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name(ovino->inference_id);

        auto it = inference_pool_.find(name);
        if (it == inference_pool_.end()) {
            std::unique_ptr<InferenceRefs> infRefs(new InferenceRefs);
            ++infRefs->numRefs;
            infRefs->proxy = nullptr;
            if (ovino->model) {
                // save master element to indicate that this element has full properties set
                infRefs->masterElement = ovino;
            } else {
                // lazy initialization
                infRefs->elementsToInit.push_back(ovino);
            }
            inference_pool_.insert({name, infRefs.release()});
        } else {
            InferenceRefs *infRefs = it->second;
            if (!infRefs) {
                throw std::runtime_error("'infRefs' is set to NULL.");
            }
            ++infRefs->numRefs;
            if (ovino->model) {
                // save master element to indicate that this element has full properties set
                infRefs->masterElement = ovino;
            } else {
                // lazy initialization
                infRefs->elementsToInit.push_back(ovino);
            }
        }
    } catch (const std::exception &e) {
        g_set_error(error, 1, 1, "%s", CreateNestedErrorMsg(e).c_str());
    }
}

void fillElementProps(GvaBaseInference *targetElem, GvaBaseInference *masterElem, InferenceImpl *inference_impl) {
    assert(masterElem);
    targetElem->inference = inference_impl;

    COPY_GSTRING(targetElem->model, masterElem->model);
    COPY_GSTRING(targetElem->device, masterElem->device);
    COPY_GSTRING(targetElem->model_proc, masterElem->model_proc);
    targetElem->batch_size = masterElem->batch_size;
    targetElem->every_nth_frame = masterElem->every_nth_frame;
    targetElem->adaptive_skip = masterElem->adaptive_skip;
    targetElem->nireq = masterElem->nireq;
    targetElem->cpu_streams = masterElem->cpu_streams;
    targetElem->gpu_streams = masterElem->gpu_streams;
    COPY_GSTRING(targetElem->infer_config, masterElem->infer_config);
    COPY_GSTRING(targetElem->allocator_name, masterElem->allocator_name);
    COPY_GSTRING(targetElem->pre_proc_name, masterElem->pre_proc_name);
    // no need to copy inference_id because it should match already.
}

void initExistingElements(InferenceRefs *infRefs) {
    if (!infRefs->masterElement) {
        g_error(
            "There is no master element. Please, check if all of mandatory parameters are set, for example 'model'.");
    }
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

        // Current ovino element with ovino->inference-id has not been registered
        assert(it != inference_pool_.end());

        infRefs = it->second;
        // if ovino is not master element, it will get all master element's properties here
        initExistingElements(infRefs);

        if (infRefs->proxy == nullptr)                 // no instance for current inference-id acquired yet
            infRefs->proxy = new InferenceImpl(ovino); // one instance for all elements with same inference-id

        return infRefs->proxy;
    } catch (const std::exception &e) {
        g_set_error(error, 1, 1, "%s", CreateNestedErrorMsg(e).c_str());
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

GstFlowReturn frame_to_base_inference(GvaBaseInference *element, GstBuffer *buf, GstVideoInfo *info) {
    if (!element || !element->inference) {
        GST_ERROR_OBJECT(element, "empty inference instance!!!!");
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }

    GstFlowReturn status;
    try {
        status = ((InferenceImpl *)element->inference)->TransformFrameIp(element, buf, info);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(element, "%s", CreateNestedErrorMsg(e).c_str());
        status = GST_FLOW_ERROR;
    }

    return status;
}

void base_inference_sink_event(GvaBaseInference *ovino, GstEvent *event) {
    try {
        if (ovino->inference) {
            ((InferenceImpl *)ovino->inference)->SinkEvent(event);
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(ovino, "%s", CreateNestedErrorMsg(e).c_str());
    }
}

void flush_inference(GvaBaseInference *ovino) {
    if (!ovino || !ovino->inference) {
        GST_ERROR_OBJECT(ovino, "empty inference instance!!!!");
        return;
    }

    ((InferenceImpl *)ovino->inference)->FlushInference();
}
