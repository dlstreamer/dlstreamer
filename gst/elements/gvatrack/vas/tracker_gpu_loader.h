/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "so_loader.h"
#include "vas/ot.h"
#include <va/va.h>

class VasOtGPULibBinderImpl {
  public:
    VasOtGPULibBinderImpl();

    bool is_loaded() const;

    std::unique_ptr<vas::ot::ObjectTracker::Builder> createBuilder() const;
    std::unique_ptr<vas::ot::ObjectTracker> createGPUTracker(vas::ot::ObjectTracker::Builder *builder,
                                                             VADisplay display,
                                                             vas::ot::TrackingType tracking_type) const;
    std::vector<vas::ot::Object>
    runTrackGPU(vas::ot::ObjectTracker *tracker, VASurfaceID surface_id, uint64_t width, uint64_t height,
                const std::vector<vas::ot::DetectedObject> &detected_objects = std::vector<vas::ot::DetectedObject>());

  private:
    SharedObject::Ptr m_libvasot_gpu_loader = nullptr;
};

class VasOtGPULibBinder {
  public:
    static VasOtGPULibBinderImpl &get();

  private:
    VasOtGPULibBinder();
};
