/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"

#include <gst/video/gstvideometa.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

class InferenceFrame;

namespace post_processing {

using TensorsTable = std::vector<std::vector<GstStructure *>>;
using OutputBlobs = std::map<std::string, InferenceBackend::OutputBlob::Ptr>;
using InferenceFrames = std::vector<std::shared_ptr<InferenceFrame>>;

struct ModelImageInputInfo {
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = -1;
    int memory_type = -1;
};

/**
 * Compares to tensors_batch of type GstVideoRegionOfInterestMeta by roi_type and coordinates.
 *
 * @param[in] left - pointer to first GstVideoRegionOfInterestMeta operand.
 * @param[in] right - pointer to second GstVideoRegionOfInterestMeta operand.
 *
 * @return true if given tensors_batch are equal, false otherwise.
 */
inline bool sameRegion(const GstVideoRegionOfInterestMeta *left, const GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

} // namespace post_processing
