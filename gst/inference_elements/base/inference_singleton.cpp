/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_singleton.h"

#include "gva_base_inference.h"
#include "inference_impl.h"
#include "utils.h"
#include <assert.h>

struct InferenceRefs {
    unsigned int numRefs = 0;
    std::list<GvaBaseInference *> elementsToInit;
    GvaBaseInference *masterElement = nullptr;
    InferenceImpl *proxy = nullptr;
    GstVideoFormat videoFormat = GST_VIDEO_FORMAT_UNKNOWN;
};

static std::map<std::string, InferenceRefs *> inference_pool_;
static std::mutex inference_pool_mutex_;

#define COPY_GSTRING(_DST, _SRC)                                                                                       \
    g_free(_DST);                                                                                                      \
    _DST = g_strdup(_SRC);

gboolean registerElement(GvaBaseInference *base_inference) {
    assert(base_inference != nullptr && "Expected a valid pointer to gva_base_inference");
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name(base_inference->model_instance_id);

        auto it = inference_pool_.find(name);
        if (it == inference_pool_.end()) {
            std::unique_ptr<InferenceRefs> infRefs(new InferenceRefs);
            ++infRefs->numRefs;
            infRefs->proxy = nullptr;
            if (base_inference->model) {
                // save master element to indicate that this element has full properties set
                infRefs->masterElement = base_inference;
            } else {
                // lazy initialization
                infRefs->elementsToInit.push_back(base_inference);
            }
            inference_pool_.insert({name, infRefs.release()});
        } else {
            InferenceRefs *infRefs = it->second;
            if (!infRefs) {
                throw std::runtime_error("'infRefs' is set to NULL.");
            }
            ++infRefs->numRefs;
            if (base_inference->model) {
                // save master element to indicate that this element has full properties set
                infRefs->masterElement = base_inference;
            } else {
                // lazy initialization
                infRefs->elementsToInit.push_back(base_inference);
            }
        }
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, INIT, ("base_inference based element registration failed"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
        return FALSE;
    }
    return TRUE;
}

void fillElementProps(GvaBaseInference *targetElem, GvaBaseInference *masterElem, InferenceImpl *inference_impl) {
    assert(targetElem && masterElem && inference_impl);
    targetElem->inference = inference_impl;

    COPY_GSTRING(targetElem->model, masterElem->model);
    COPY_GSTRING(targetElem->device, masterElem->device);
    COPY_GSTRING(targetElem->model_proc, masterElem->model_proc);
    targetElem->batch_size = masterElem->batch_size;
    targetElem->inference_interval = masterElem->inference_interval;
    targetElem->no_block = masterElem->no_block;
    targetElem->nireq = masterElem->nireq;
    targetElem->cpu_streams = masterElem->cpu_streams;
    targetElem->gpu_streams = masterElem->gpu_streams;
    COPY_GSTRING(targetElem->ie_config, masterElem->ie_config);
    COPY_GSTRING(targetElem->allocator_name, masterElem->allocator_name);
    COPY_GSTRING(targetElem->pre_proc_type, masterElem->pre_proc_type);
    COPY_GSTRING(targetElem->object_class, masterElem->object_class);
    // no need to copy model_instance_id because it should match already.
}

void initExistingElements(InferenceRefs *infRefs) {
    if (not infRefs->masterElement) {
        throw std::logic_error("There is no master inference element. Please, check if all of mandatory parameters are "
                               "set, for example 'model'.");
    }
    for (auto elem : infRefs->elementsToInit) {
        fillElementProps(elem, infRefs->masterElement, infRefs->proxy);
    }
}

void check_image_formats_same(GstVideoFormat &existing_format, const GstVideoFormat &received_format) {
    if (existing_format == GST_VIDEO_FORMAT_UNKNOWN)
        existing_format = received_format;
    else if (existing_format != received_format) {
        std::string err_msg =
            "All image formats for the same model-instance-id in multichannel mode must be the same. The current image "
            "format of this inference element in caps is " +
            std::string(gst_video_format_to_string(received_format)) +
            " , but the first one accepted in another inference element is " +
            std::string(gst_video_format_to_string(existing_format)) +
            ". Try converting video frames to one image format in each channel in front of inference elements using "
            "various converters, or use different model-instance-id for each channel";
        throw std::logic_error(err_msg);
    }
}

InferenceImpl *acquire_inference_instance(GvaBaseInference *base_inference) {
    try {
        if (!base_inference)
            throw std::invalid_argument("GvaBaseInference is null");

        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name(base_inference->model_instance_id);

        InferenceRefs *infRefs = nullptr;
        auto it = inference_pool_.find(name);

        // Current base_inference element with base_inference->inference-id has not been registered
        assert(it != inference_pool_.end());

        infRefs = it->second;
        check_image_formats_same(infRefs->videoFormat, base_inference->info->finfo->format);
        // if base_inference is not master element, it will get all master element's properties here
        initExistingElements(infRefs);

        if (infRefs->proxy == nullptr)                          // no instance for current inference-id acquired yet
            infRefs->proxy = new InferenceImpl(base_inference); // one instance for all elements with same inference-id

        return infRefs->proxy;
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, INIT, ("base_inference plugin intitialization failed"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
        return nullptr;
    }
}

void release_inference_instance(GvaBaseInference *base_inference) {
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name(base_inference->model_instance_id);

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
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, SHUTDOWN, ("base_inference failed on releasing inference instance"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
    }
}
