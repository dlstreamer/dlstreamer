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

const std::string BoxesLabelsConverter::boxes_layer_name = "boxes";
const std::string BoxesLabelsConverter::labels_layer_name = "labels";

void BoxesLabelsConverter::parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &boxes_blob,
                                           const InferenceBackend::OutputBlob::Ptr &labels_blob,
                                           DetectedObjectsTable &objects_table,
                                           const ModelImageInputInfo &model_input_image_info, double roi_scale) const {
    if (!boxes_blob or !labels_blob)
        throw std::invalid_argument("Output blob is nullptr.");

    const float *boxes_data = reinterpret_cast<const float *>(boxes_blob->GetData());
    const int32_t *labels_data = reinterpret_cast<const int32_t *>(labels_blob->GetData());

    if (!boxes_data or !labels_data)
        throw std::invalid_argument("Output blob data is nullptr.");

    const auto &boxes_dims = boxes_blob->GetDims();
    const auto &labels_dims = labels_blob->GetDims();

    size_t boxes_dims_size = boxes_dims.size();

    if (boxes_dims_size < BlobToROIConverter::min_dims_size)
        throw std::invalid_argument("Output blob dimentions size " + std::to_string(boxes_dims_size) +
                                    " is not supported (less than " +
                                    std::to_string(BlobToROIConverter::min_dims_size) + ").");

    size_t object_size = boxes_dims[boxes_dims_size - 1];
    if (object_size != BoxesLabelsConverter::model_object_size)
        throw std::invalid_argument("Object size dimension of output blob is set to " + std::to_string(object_size) +
                                    ", but only " + std::to_string(BoxesLabelsConverter::model_object_size) +
                                    " supported.");

    size_t max_proposal_count = boxes_dims[0];
    if (max_proposal_count != labels_dims[0])
        throw std::invalid_argument("Output blobs have different numbers of detected bounding boxes.");

    for (size_t i = 0; i < max_proposal_count; ++i) {

        float confidence = boxes_data[i * object_size + 4];

        /* discard inference results that do not match 'confidence_threshold' */
        if (confidence < confidence_threshold) {
            continue;
        }

        float bbox_x = boxes_data[i * object_size + 0] / model_input_image_info.width;
        float bbox_y = boxes_data[i * object_size + 1] / model_input_image_info.height;
        float bbox_w = boxes_data[i * object_size + 2] / model_input_image_info.width - bbox_x;
        float bbox_h = boxes_data[i * object_size + 3] / model_input_image_info.height - bbox_y;

        const size_t label_id = safe_convert<size_t>(labels_data[i]);

        // apply roi_scale if set
        if (roi_scale > 0 && roi_scale != 1) {
            bbox_x = bbox_x + bbox_w / 2 * (1 - roi_scale);
            bbox_y = bbox_y + bbox_h / 2 * (1 - roi_scale);
            bbox_w = bbox_w * roi_scale;
            bbox_h = bbox_h * roi_scale;
        }

        DetectedObject bbox(bbox_x, bbox_y, bbox_w, bbox_h, confidence, label_id, getLabelByLabelId(label_id));
        objects_table[0].push_back(bbox);
    }
}
TensorsTable BoxesLabelsConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        DetectedObjectsTable objects_table(model_input_image_info.batch_size);
        const auto &detection_result = getModelProcOutputInfo();

        if (detection_result == nullptr)
            throw std::invalid_argument("detection_result tensor is nullptr");
        double roi_scale = 1.0;
        gst_structure_get_double(detection_result.get(), "roi_scale", &roi_scale);

        InferenceBackend::OutputBlob::Ptr boxes_blob = output_blobs.at(boxes_layer_name);
        InferenceBackend::OutputBlob::Ptr labels_blob = output_blobs.at(labels_layer_name);

        parseOutputBlob(boxes_blob, labels_blob, objects_table, model_input_image_info, roi_scale);

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do ATSS post-processing."));
    }
    return TensorsTable{};
}
