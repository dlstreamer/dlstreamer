/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_v3.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"

#include <gst/gst.h>

#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace post_processing;

std::vector<float> YOLOv3Converter::softmax(const float *arr, size_t size, size_t common_offset,
                                            size_t side_square) const {
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        const size_t class_index = entryIndex(side_square, common_offset, 5 + i);
        sftm_arr[i] = std::exp(arr[class_index]);
        sum += sftm_arr[i];
    }
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] /= sum;
    }
    return sftm_arr;
}

void YOLOv3Converter::parseOutputBlob(const float *blob_data, const std::vector<size_t> &blob_dims, size_t blob_size,
                                      std::vector<DetectedObject> &objects) const {
    size_t side_w = output_shape_info.cells_number_x;
    size_t side_h = output_shape_info.cells_number_y;

    switch (output_dims_layout) {
    case OutputDimsLayout::NBCxCy: {
        side_w = blob_dims[2];
        side_h = blob_dims[3];
    } break;

    case OutputDimsLayout::NCxCyB: {
        side_w = blob_dims[1];
        side_h = blob_dims[2];
    } break;

    case OutputDimsLayout::CxCyB: {
        side_w = blob_dims[0];
        side_h = blob_dims[1];
    } break;

    case OutputDimsLayout::BCxCy: {
        side_w = blob_dims[1];
        side_h = blob_dims[2];
    } break;

    case OutputDimsLayout::NO: {
        size_t multiplier =
            blob_size / (output_shape_info.cells_number_x * output_shape_info.cells_number_y *
                         output_shape_info.bbox_number_on_cell * (5 + output_shape_info.classes_number));
        multiplier = safe_convert<size_t>(std::sqrt(safe_convert<double>(multiplier)));

        side_w *= multiplier;
        side_h *= multiplier;
    } break;

    default:
        throw std::runtime_error("Unsupported output layout.");
    }

    const std::vector<size_t> &mask = masks.at(std::min(side_w, side_h));

    if (not blob_data)
        throw std::invalid_argument("Output blob data is nullptr.");

    size_t input_width = getModelInputImageInfo().width;
    size_t input_height = getModelInputImageInfo().height;
    const size_t side_square = side_w * side_h;

    for (size_t i = 0; i < side_square; ++i) {
        const size_t row = i / side_w;
        const size_t col = i % side_w;
        for (size_t bbox_cell_num = 0; bbox_cell_num < output_shape_info.bbox_number_on_cell; ++bbox_cell_num) {
            const size_t common_offset = bbox_cell_num * side_square + i;
            const size_t bbox_conf_index = entryIndex(side_square, common_offset, coords);
            const size_t bbox_index = entryIndex(side_square, common_offset, 0);

            float bbox_conf = blob_data[bbox_conf_index];
            if (output_sigmoid_activation)
                bbox_conf = sigmoid(bbox_conf);
            if (bbox_conf < confidence_threshold)
                continue;

            std::pair<size_t, float> bbox_class = std::make_pair(0, 0.f);
            if (do_cls_softmax) {
                const auto cls_confs = softmax(blob_data, output_shape_info.classes_number, common_offset, side_square);

                for (size_t bbox_class_id = 0; bbox_class_id < output_shape_info.classes_number; ++bbox_class_id) {
                    const float bbox_class_prob = cls_confs[bbox_class_id];

                    if (bbox_class_prob > 1.f && bbox_class_prob < 0.f) {
                        GST_WARNING("bbox_class_prob is weird %f.", bbox_class_prob);
                    }
                    if (bbox_class_prob > bbox_class.second) {
                        bbox_class.first = bbox_class_id;
                        bbox_class.second = bbox_class_prob;
                    }
                }
            } else {
                for (size_t bbox_class_id = 0; bbox_class_id < output_shape_info.classes_number; ++bbox_class_id) {
                    const size_t class_index = entryIndex(side_square, common_offset, 5 + bbox_class_id);
                    const float bbox_class_prob = blob_data[class_index];

                    if (bbox_class_prob > 1.f && bbox_class_prob < 0.f) {
                        GST_WARNING("bbox_class_prob is weird %f.", bbox_class_prob);
                    }
                    if (bbox_class_prob > bbox_class.second) {
                        bbox_class.first = bbox_class_id;
                        bbox_class.second = bbox_class_prob;
                    }
                }
            }

            const float confidence = bbox_conf * bbox_class.second;
            if (confidence < confidence_threshold)
                continue;

            // TODO: check if index in array range
            const float raw_x = blob_data[bbox_index + 0 * side_square];
            const float raw_y = blob_data[bbox_index + 1 * side_square];
            const float raw_w = blob_data[bbox_index + 2 * side_square];
            const float raw_h = blob_data[bbox_index + 3 * side_square];

            const float x =
                static_cast<float>(col + (output_sigmoid_activation ? sigmoid(raw_x) : raw_x)) / side_w * input_width;
            const float y =
                static_cast<float>(row + (output_sigmoid_activation ? sigmoid(raw_y) : raw_y)) / side_h * input_height;

            // TODO: check if index in array range
            const size_t anchor_offset = 2 * mask[0];
            const float width = std::exp(raw_w) * anchors[anchor_offset + 2 * bbox_cell_num];
            const float height = std::exp(raw_h) * anchors[anchor_offset + 2 * bbox_cell_num + 1];

            DetectedObject bbox(x, y, width, height, confidence, bbox_class.first,
                                BlobToMetaConverter::getLabelByLabelId(bbox_class.first), 1.0f / input_width,
                                1.0f / input_height, true);
            objects.push_back(bbox);
        }
    }
}

size_t YOLOv3Converter::entryIndex(size_t side_square, size_t location, size_t entry) const {
    size_t bbox_cell_num = location / side_square;
    size_t loc = location % side_square;
    // side_square is the tensor dimension of the YoloV3 model. Overflow is not possible here.
    return side_square * (bbox_cell_num * (output_shape_info.classes_number + 5) + entry) + loc;
}
