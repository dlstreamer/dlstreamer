/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/yolo_v3_base.h"

#include "inference_backend/logger.h"

#include <cmath>

using namespace DetectionPlugin;
using namespace Converters;

YOLOV3Converter::YOLOV3Converter(size_t classes_number, std::vector<float> anchors,
                                 std::map<size_t, std::vector<size_t>> masks, double iou_threshold, size_t num,
                                 size_t coords, float input_size)
    : YOLOConverter(anchors, iou_threshold), classes_number(classes_number), masks(masks), coords(coords), num(num),
      input_size(input_size) {
}

bool YOLOV3Converter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                              const std::vector<std::shared_ptr<InferenceFrame>> &frames,
                              GstStructure *detection_result, double confidence_threshold, GValueArray *labels) {
    bool flag = false;
    try {
        if (frames.size() != 1) {
            std::string err = "Batch size other than 1 is not supported";
            const gchar *converter = gst_structure_get_string(detection_result, "converter");
            if (converter)
                err += " for this post processor: " + std::string(converter);
            throw std::invalid_argument(err);
        }

        std::vector<DetectedObject> objects;
        for (const auto &blob_iter : output_blobs) {
            InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            auto dims = blob->GetDims();
            if (dims.size() != 4 or dims[2] != dims[3]) {
                throw std::runtime_error("Invalid output blob dimensions");
            }
            const int side = dims[2];
            int anchor_offset = 0;
            std::vector<size_t> mask = masks.at(side);
            anchor_offset = 2 * mask[0];
            const float *output_blob = (const float *)blob->GetData();
            if (not output_blob)
                throw std::invalid_argument("Output blob data is nullptr");

            const uint32_t side_square = side * side;
            for (uint32_t i = 0; i < side_square; ++i) {
                const int row = i / side;
                const int col = i % side;
                for (uint32_t n = 0; n < num; ++n) {
                    const int obj_index = entryIndex(side, coords, classes_number, n * side_square + i, coords);
                    const int box_index = entryIndex(side, coords, classes_number, n * side_square + i, 0);

                    const float scale = output_blob[obj_index];
                    if (scale < confidence_threshold)
                        continue;
                    // TODO: check if index in array range
                    const float x = (col + output_blob[box_index + 0 * side_square]) / side * input_size;
                    const float y = (row + output_blob[box_index + 1 * side_square]) / side * input_size;

                    // TODO: check if index in array range
                    const float width =
                        std::exp(output_blob[box_index + 2 * side_square]) * anchors[anchor_offset + 2 * n];
                    const float height =
                        std::exp(output_blob[box_index + 3 * side_square]) * anchors[anchor_offset + 2 * n + 1];

                    for (uint32_t j = 0; j < classes_number; ++j) {
                        const int class_index =
                            entryIndex(side, coords, classes_number, n * side_square + i, coords + 1 + j);
                        const float prob = scale * output_blob[class_index];
                        if (prob < confidence_threshold)
                            continue;
                        DetectedObject obj(x, y, width, height, j, prob, 1 / input_size, 1 / input_size);
                        objects.push_back(obj);
                    }
                }
            }
        }
        storeObjects(objects, frames[0], detection_result, labels);
        flag = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloV3 post-processing"));
    }
    return flag;
}
uint32_t YOLOV3Converter::entryIndex(uint32_t side, uint32_t lcoords, uint32_t lclasses, uint32_t location,
                                     uint32_t entry) {
    uint32_t side_square = side * side;
    uint32_t n = location / side_square;
    uint32_t loc = location % side_square;
    // side_square is the tensor dimension of the YoloV3 model. Overflow is not possible here.
    return side_square * (n * (lcoords + lclasses + 1) + entry) + loc;
}
