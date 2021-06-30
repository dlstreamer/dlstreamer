/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ov_default.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

void OVDefaultConverter::parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &blob, DetectedObjectsTable &objects,
                                         double roi_scale) const {
    const float *data = reinterpret_cast<const float *>(blob->GetData());
    if (not data)
        throw std::invalid_argument("Output blob data is nullptr");

    auto dims = blob->GetDims();
    size_t dims_size = dims.size();

    static constexpr size_t min_dims_size = 2;
    if (dims_size < min_dims_size)
        throw std::invalid_argument("Output blob dimentions size " + std::to_string(dims_size) +
                                    " is not supported (less than " + std::to_string(min_dims_size) + ")");

    for (size_t i = min_dims_size + 1; i < dims_size; ++i) {
        if (dims[dims_size - i] != 1)
            throw std::invalid_argument("All output blob dimensions, except for object size and max "
                                        "objects count, must be equal to 1");
    }

    size_t object_size = dims[dims_size - 1];
    static constexpr size_t supported_object_size = 7; // SSD DetectionOutput format
    if (object_size != supported_object_size)
        throw std::invalid_argument("Object size dimension of output blob is set to " + std::to_string(object_size) +
                                    ", but only " + std::to_string(supported_object_size) + " supported");

    size_t max_proposal_count = dims[dims_size - 2];
    for (size_t i = 0; i < max_proposal_count; ++i) {
        int image_id = safe_convert<int>(data[i * object_size + 0]);
        /* check if 'image_id' contains a valid index for 'frames' vector */
        if (image_id < 0 || (size_t)image_id >= objects.size()) {
            break;
        }

        int label_id = safe_convert<int>(data[i * object_size + 1]);
        double confidence = data[i * object_size + 2];
        /* discard inference results that do not match 'confidence_threshold' */
        if (confidence < confidence_threshold) {
            continue;
        }

        float bbox_x = data[i * object_size + 3];
        float bbox_y = data[i * object_size + 4];
        float bbox_w = data[i * object_size + 5] - bbox_x;
        float bbox_h = data[i * object_size + 6] - bbox_y;

        // TODO: in future we must return to use GVA::VideoFrame
        // GVA::VideoFrame video_frame(frames[image_id]->buffer, frames[image_id]->info);

        // TODO: check if we can simplify below code further
        // apply roi_scale if set
        if (roi_scale > 0 && roi_scale != 1) {
            bbox_x = bbox_x + bbox_w / 2 * (1 - roi_scale);
            bbox_y = bbox_y + bbox_h / 2 * (1 - roi_scale);
            bbox_w = bbox_w * roi_scale;
            bbox_h = bbox_h * roi_scale;
        }

        DetectedObject bbox(bbox_x, bbox_y, bbox_w, bbox_h, confidence, label_id, getLabelByLabelId(label_id));
        objects[image_id].push_back(bbox);
    }
}

TensorsTable OVDefaultConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        DetectedObjectsTable objects(model_input_image_info.batch_size);

        const auto &detection_result = getModelProcOutputInfo();

        if (detection_result == nullptr)
            throw std::invalid_argument("detection_result tensor is nullptr");
        double roi_scale = 1.0;
        gst_structure_get_double(detection_result.get(), "roi_scale", &roi_scale);

        // Check whether we can handle this blob instead iterator
        for (const auto &blob_iter : output_blobs) {
            const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            parseOutputBlob(blob, objects, roi_scale);
        }

        return storeObjects(objects);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SSD post-processing"));
    }
    return TensorsTable{};
}
