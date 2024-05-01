/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_singleton.h"
#include "gva_base_inference_priv.hpp"
#include "inference_backend/buffer_mapper.h"

#include "gva_base_inference.h"
#include "inference_impl.h"
#include "utils.h"
#include <assert.h>
#include <set>

struct InferenceRefs {
    std::set<GvaBaseInference *> refs;
    InferenceImpl *proxy = nullptr;
    dlstreamer::ContextPtr context = nullptr;
    GstVideoFormat videoFormat = GST_VIDEO_FORMAT_UNKNOWN;
    CapsFeature capsFeature = ANY_CAPS_FEATURE;
};

static std::map<std::string, InferenceRefs *> inference_pool_;
static std::mutex inference_pool_mutex_;

std::string capsFeatureString(CapsFeature newCapsFeature);
std::string get_inference_key(GvaBaseInference *base_inference) {
    return std::string(base_inference->model_instance_id) + "_" + capsFeatureString(base_inference->caps_feature);
}

#define COPY_GSTRING(_DST, _SRC)                                                                                       \
    g_free(_DST);                                                                                                      \
    _DST = g_strdup(_SRC);

void addBaseInferenceToInfRes(InferenceRefs *infRefs, GvaBaseInference *base_inference) {
    infRefs->refs.insert(base_inference);
    GST_INFO_OBJECT(base_inference, "increment numref: refs size = %lu\n", infRefs->refs.size());
}

InferenceRefs *registerElementUnlocked(GvaBaseInference *base_inference) {
    std::string name = get_inference_key(base_inference);
    GST_INFO_OBJECT(base_inference, "key: %s\n", name.c_str());
    auto it = inference_pool_.find(name);
    if (it == inference_pool_.end()) {
        InferenceRefs *infRefs = new InferenceRefs();
        addBaseInferenceToInfRes(infRefs, base_inference);
        inference_pool_[name] = infRefs;
        return infRefs;
    }
    InferenceRefs *infRefs = it->second;
    if (!infRefs) {
        throw std::runtime_error("'infRefs' is set to NULL.");
    }
    addBaseInferenceToInfRes(infRefs, base_inference);
    return infRefs;
}

gboolean registerElement(GvaBaseInference *base_inference) {
    assert(base_inference != nullptr && "Expected a valid pointer to gva_base_inference");
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        registerElementUnlocked(base_inference);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, INIT, ("base_inference based element registration failed"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
        return FALSE;
    }
    return TRUE;
}

void fillElementProps(GvaBaseInference *targetElem, GvaBaseInference *masterElem, InferenceImpl *inference_impl) {
    assert(targetElem);
    assert(masterElem);
    UNUSED(inference_impl);

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
    COPY_GSTRING(targetElem->labels, masterElem->labels);
    // no need to copy model_instance_id because it should match already.
}

void initExistingElements(InferenceRefs *infRefs) {
    GvaBaseInference *master = nullptr;
    for (auto elem : infRefs->refs) {
        if (elem->model && *elem->model != 0) {
            master = elem;
            break;
        }
    }

    if (!master) {
        throw std::logic_error("There is no master inference element. Please, check if all of mandatory parameters are "
                               "set, for example 'model'.");
    }

    for (auto elem : infRefs->refs) {
        if (elem != master)
            fillElementProps(elem, master, infRefs->proxy);
    }
}

std::string capsFeatureString(CapsFeature newCapsFeature) {
    switch (newCapsFeature) {
    case ANY_CAPS_FEATURE:
        return "ANY";
    case SYSTEM_MEMORY_CAPS_FEATURE:
        return "System";
    case VA_SURFACE_CAPS_FEATURE:
        return "VASurface";
    case VA_MEMORY_CAPS_FEATURE:
        return "VAMemory";
    case DMA_BUF_CAPS_FEATURE:
        return "DMABuf";
    }
    return "";
}

void initInferenceProps(InferenceRefs &inferenceRefs, GstVideoFormat newFormat, CapsFeature newCapsFeature) {
    if (inferenceRefs.videoFormat == GST_VIDEO_FORMAT_UNKNOWN)
        inferenceRefs.videoFormat = newFormat;
    if (inferenceRefs.capsFeature == CapsFeature::ANY_CAPS_FEATURE)
        inferenceRefs.capsFeature = newCapsFeature;
}

void check_inference_props_same(const InferenceRefs &inferenceRefs, GstVideoFormat newFormat,
                                CapsFeature newCapsFeature) {
    if (inferenceRefs.videoFormat != newFormat || inferenceRefs.capsFeature != newCapsFeature) {
        std::string err_msg =
            "All image formats and memoryType for the same model-instance-id in multichannel mode must be the same."
            " The current image "
            "format and memory type of this inference element in caps is " +
            std::string(gst_video_format_to_string(newFormat)) + " and " + capsFeatureString(newCapsFeature) +
            " , but the first one accepted in another inference element is " +
            std::string(gst_video_format_to_string(inferenceRefs.videoFormat)) + " and " +
            capsFeatureString(inferenceRefs.capsFeature) +
            ". Try converting video frames to one image format in each channel in front of inference elements using "
            "various converters, or use different model-instance-id for each channel"
            "try to fixate format and memory type using capsfilter";
        throw std::logic_error(err_msg);
    }
}

InferenceImpl *acquire_inference_instance(GvaBaseInference *base_inference) {
    try {
        if (!base_inference)
            throw std::invalid_argument("GvaBaseInference is null");

        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        InferenceRefs *infRefs = nullptr;
        std::string name = get_inference_key(base_inference);
        GST_INFO_OBJECT(base_inference, "key: %s\n", name.c_str());
        infRefs = registerElementUnlocked(base_inference);

        initInferenceProps(*infRefs, base_inference->info->finfo->format, base_inference->caps_feature);
        check_inference_props_same(*infRefs, base_inference->info->finfo->format, base_inference->caps_feature);

        // if base_inference is not master element, it will get all master element's properties here
        initExistingElements(infRefs);

        if (infRefs->proxy == nullptr) {                        // no instance for current inference-id acquired yet
            infRefs->proxy = new InferenceImpl(base_inference); // one instance for all elements with same inference-id
            infRefs->context = base_inference->priv->va_display;
        } else {
            base_inference->priv->va_display = infRefs->context;
        }

        return infRefs->proxy;
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, INIT, ("base_inference plugin initialization failed"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
        return nullptr;
    }
}

void release_inference_instance(GvaBaseInference *base_inference) {
    try {
        std::lock_guard<std::mutex> guard(inference_pool_mutex_);
        std::string name = get_inference_key(base_inference);
        GST_INFO_OBJECT(base_inference, "key: %s\n", name.c_str());

        auto it = inference_pool_.find(name);
        if (it == inference_pool_.end())
            return;

        InferenceRefs *infRefs = it->second;
        infRefs->refs.erase(base_inference);
        if (infRefs->refs.empty()) {
            delete infRefs->proxy;
            delete infRefs;
            inference_pool_.erase(name);
        }
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_inference, LIBRARY, SHUTDOWN, ("base_inference failed on releasing inference instance"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
    }
}
