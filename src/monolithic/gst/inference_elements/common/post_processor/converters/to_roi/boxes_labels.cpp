/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "boxes_labels.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

const std::string BoxesLabelsConverter::labels_layer_name = "labels";

InferenceBackend::OutputBlob::Ptr BoxesLabelsConverter::getLabelsScoresBlob(const OutputBlobs &output_blobs) const {
    return output_blobs.at(labels_layer_name);
}

std::pair<size_t, float>
BoxesLabelsConverter::getLabelIdConfidence(const InferenceBackend::OutputBlob::Ptr &labels_blob, size_t bbox_i,
                                           float conf) const {
    if (!labels_blob)
        throw std::runtime_error("Output blob is nullptr.");

    const uint32_t *labels_data = reinterpret_cast<const uint32_t *>(labels_blob->GetData());
    if (!labels_data)
        throw std::runtime_error("Output blob data is nullptr.");

    return std::make_pair(safe_convert<size_t>(labels_data[bbox_i]), conf);
}

bool BoxesLabelsConverter::isValidModelOutputs(const std::map<std::string, std::vector<size_t>> &model_outputs_info) {
    if (!BoxesLabelsScoresConverter::isValidModelBoxesOutput(model_outputs_info))
        return false;

    return BoxesLabelsScoresConverter::isValidModelAdditionalOutput(model_outputs_info, labels_layer_name);
}
