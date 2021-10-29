/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_gpu_loader.h"
#include <inference_backend/logger.h>

using namespace vas::ot;

VasOtGPULibBinderImpl::VasOtGPULibBinderImpl() {
    try {
        m_libvasot_gpu_loader = SharedObject::getLibrary("libvasot_gpu.so");
    } catch (const std::exception &e) {
        m_libvasot_gpu_loader = nullptr;
        GVA_ERROR("Couldn't load shared library for GPU OT: %s", e.what());
    }
}

bool VasOtGPULibBinderImpl::is_loaded() const {
    return m_libvasot_gpu_loader != nullptr;
}

std::unique_ptr<ObjectTracker::Builder> VasOtGPULibBinderImpl::createBuilder() const {
    if (!is_loaded())
        return nullptr;
    return m_libvasot_gpu_loader->invoke<std::unique_ptr<ObjectTracker::Builder>()>("CreateBuilder");
}

std::unique_ptr<ObjectTracker> VasOtGPULibBinderImpl::createGPUTracker(ObjectTracker::Builder *builder,
                                                                       VADisplay display, TrackingType type) const {
    if (!is_loaded())
        return nullptr;
    return m_libvasot_gpu_loader
        ->invoke<std::unique_ptr<ObjectTracker>(ObjectTracker::Builder *, VADisplay, TrackingType)>(
            "BuildGPUTracker", builder, display, type);
}

std::vector<Object> VasOtGPULibBinderImpl::runTrackGPU(ObjectTracker *tracker, VASurfaceID surfaceId, uint64_t width,
                                                       uint64_t height,
                                                       const std::vector<DetectedObject> &detectedObjects) {
    if (!is_loaded())
        return std::vector<Object>();

    return m_libvasot_gpu_loader
        ->invoke<std::vector<Object>(ObjectTracker *, VASurfaceID, uint64_t, uint64_t, std::vector<DetectedObject>)>(
            "RunTrackGPU", tracker, surfaceId, width, height, detectedObjects);
}

VasOtGPULibBinderImpl &VasOtGPULibBinder::get() {
    static VasOtGPULibBinderImpl instance;
    return instance;
}
