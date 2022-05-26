/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "frame_wrapper.h"
#include "inference_backend/image_inference.h"

#include <gst/video/gstvideometa.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

using TensorsTable = std::vector<std::vector<GstStructure *>>;
using OutputBlobs = std::map<std::string, InferenceBackend::OutputBlob::Ptr>;
// <layer_name, blob_dims>
using ModelOutputsInfo = std::map<std::string, std::vector<size_t>>;

enum class ConverterType { TO_ROI, TO_TENSOR, RAW };
enum class AttachType { TO_FRAME, TO_ROI, FOR_MICRO /* remove workaround when moved to micro elements */ };

struct ModelImageInputInfo {
    size_t width = 0;
    size_t height = 0;
    size_t batch_size = 0;
    int format = -1;
    int memory_type = -1;
};

void checkFramesAndTensorsTable(const FramesWrapper &frames, const TensorsTable &tensors);

/**
 * Compares to tensors_batch of type GstVideoRegionOfInterestMeta by roi_type and coordinates.
 *
 * @param[in] left - pointer to first GstVideoRegionOfInterestMeta operand.
 * @param[in] right - pointer to second GstVideoRegionOfInterestMeta operand.
 *
 * @return true if given tensors_batch are equal, false otherwise.
 */
inline bool sameRegion(const GstVideoRegionOfInterestMeta *left, GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

template <typename T>
std::pair<const T *, size_t> get_data_by_batch_index(const T *batch_data, size_t batch_data_size, size_t batch_size,
                                                     size_t batch_index) {
    if (batch_index >= batch_size)
        throw std::invalid_argument("Batch index must be less then batch size.");

    const size_t data_size = batch_data_size / batch_size;
    if (data_size * (batch_index + 1) > batch_data_size)
        throw std::invalid_argument("Invalid batch_index.");

    const T *data = batch_data + data_size * batch_index;

    return std::make_pair(data, data_size);
}

} // namespace post_processing
