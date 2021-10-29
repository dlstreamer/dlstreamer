/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class BoxesLabelsConverter : public BlobToROIConverter {
  protected:
    void parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &boxes_blob,
                         const InferenceBackend::OutputBlob::Ptr &labels_blob, DetectedObjectsTable &objects_table,
                         const ModelImageInputInfo &model_input_image_info, double roi_scale) const;

  public:
    BoxesLabelsConverter(BlobToMetaConverter::Initializer initializer, double confidence_threshold)
        : BlobToROIConverter(std::move(initializer), confidence_threshold, false, 0.0) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static const size_t model_object_size = 5; // ATSS DetectionOutput format
    static const std::string boxes_layer_name;
    static const std::string labels_layer_name;

    static bool isValidModelOutputs(const std::map<std::string, std::vector<size_t>> &model_outputs_info) {
        bool result = false;

        if (!model_outputs_info.count(boxes_layer_name) || !model_outputs_info.count(labels_layer_name))
            return result;

        const std::vector<size_t> &boxes_dims = model_outputs_info.at(boxes_layer_name);
        if (boxes_dims[boxes_dims.size() - 1] == model_object_size)
            result = true;

        return result;
    }

    static std::string getName() {
        return "boxes_labels";
    }
    static std::string getDepricatedName() {
        return "tensor_to_bbox_atss";
    }
};
} // namespace post_processing
