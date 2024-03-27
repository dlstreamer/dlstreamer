/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "boxes_scores.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

const std::string BoxesScoresConverter::scores_layer_name = "scores";

namespace {

std::vector<float> softmax(const float *arr, size_t size) {
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] = std::exp(arr[i]);
        sum += sftm_arr[i];
    }
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] /= sum;
    }
    return sftm_arr;
}
} // namespace

InferenceBackend::OutputBlob::Ptr BoxesScoresConverter::getLabelsScoresBlob(const OutputBlobs &output_blobs) const {
    return output_blobs.at(scores_layer_name);
}

std::pair<size_t, float>
BoxesScoresConverter::getLabelIdConfidence(const InferenceBackend::OutputBlob::Ptr &scores_blob, size_t bbox_i,
                                           float conf) const {
    if (!scores_blob)
        throw std::invalid_argument("Output blob is nullptr.");

    if (!scores_blob->GetData())
        throw std::runtime_error("Output blob data is nullptr.");

    if (scores_blob->GetPrecision() != InferenceBackend::Blob::Precision::FP32)
        throw std::runtime_error("Unsupported label precision.");

    const float *data = reinterpret_cast<const float *>(scores_blob->GetData());

    const auto &dims = scores_blob->GetDims();
    const size_t classes_num = dims[2];

    const size_t max_proposal_count = dims[0];
    assert(bbox_i < max_proposal_count && "bbox index greater than max proposals count.");

    const float *data_shifted = data + bbox_i * classes_num;

    if (do_cls_softmax) {
        const auto normed_data = softmax(data_shifted, classes_num);
        auto max_elem = std::max_element(normed_data.cbegin(), normed_data.cend());
        auto index = std::distance(normed_data.cbegin(), max_elem);
        return std::make_pair(safe_convert<size_t>(index), (*max_elem) * conf);
    }

    auto max_elem = std::max_element(data_shifted, data_shifted + classes_num);
    auto index = std::distance(data_shifted, max_elem);
    return std::make_pair(safe_convert<size_t>(index), (*max_elem) * conf);
}

bool BoxesScoresConverter::isValidModelOutputs(const std::map<std::string, std::vector<size_t>> &model_outputs_info) {
    if (!BoxesLabelsScoresConverter::isValidModelBoxesOutput(model_outputs_info))
        return false;

    return BoxesLabelsScoresConverter::isValidModelAdditionalOutput(model_outputs_info, scores_layer_name);
}

std::tuple<float, float, float, float> BoxesScoresConverter::getBboxCoordinates(const float *bbox_data, size_t,
                                                                                size_t) const {
    float bbox_x = bbox_data[0];
    float bbox_y = bbox_data[1];
    float bbox_w = bbox_data[2];
    float bbox_h = bbox_data[3];

    return std::make_tuple(bbox_x, bbox_y, bbox_w, bbox_h);
}
