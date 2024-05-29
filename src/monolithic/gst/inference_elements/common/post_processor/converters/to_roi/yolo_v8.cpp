/*******************************************************************************
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_v8.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;

void YOLOv8Converter::parseOutputBlob(const float *data, const std::vector<size_t> &dims,
                                      std::vector<DetectedObject> &objects, bool oob) const {

    size_t dims_size = dims.size();
    size_t input_width = getModelInputImageInfo().width;
    size_t input_height = getModelInputImageInfo().height;

    if (dims_size < BlobToROIConverter::min_dims_size)
        throw std::invalid_argument("Output blob dimensions size " + std::to_string(dims_size) +
                                    " is not supported (less than " +
                                    std::to_string(BlobToROIConverter::min_dims_size) + ").");

    size_t object_size = dims[dims_size - 2];
    size_t max_proposal_count = dims[dims_size - 1];

    // transpose
    cv::Mat outputs(object_size, max_proposal_count, CV_32F, (float *)data);
    cv::transpose(outputs, outputs);
    float *output_data = (float *)outputs.data;

    for (size_t i = 0; i < max_proposal_count; ++i) {
        float *classes_scores = output_data + YOLOV8_OFFSET_CS;
        cv::Mat scores(1, object_size - YOLOV8_OFFSET_CS - (oob ? 1 : 0), CV_32FC1, classes_scores);
        cv::Point class_id;
        double maxClassScore;
        cv::minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);
        if (maxClassScore > confidence_threshold) {

            float x = output_data[YOLOV8_OFFSET_X];
            float y = output_data[YOLOV8_OFFSET_Y];
            float w = output_data[YOLOV8_OFFSET_W];
            float h = output_data[YOLOV8_OFFSET_H];
            float r = oob ? output_data[object_size - 1] : 0;
            objects.push_back(DetectedObject(x, y, w, h, r, maxClassScore, class_id.x,
                                             BlobToMetaConverter::getLabelByLabelId(class_id.x), 1.0f / input_width,
                                             1.0f / input_height, true));
        }
        output_data += object_size;
    }
}

TensorsTable YOLOv8Converter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;

        DetectedObjectsTable objects_table(batch_size);

        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];

            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (not blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                parseOutputBlob(reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number,
                                blob->GetDims(), objects, false);
            }
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV8 post-processing."));
    }
    return TensorsTable{};
}

TensorsTable YOLOv8OBBConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;

        DetectedObjectsTable objects_table(batch_size);

        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];

            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (not blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                parseOutputBlob(reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number,
                                blob->GetDims(), objects, true);
            }
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV8-OBB post-processing."));
    }
    return TensorsTable{};
}