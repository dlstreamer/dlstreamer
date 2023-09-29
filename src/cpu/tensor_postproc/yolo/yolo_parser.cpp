/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <dlstreamer/base/dictionary.h>

#include "yolo_parser.h"

namespace dlstreamer {

const TensorInfo &YoloParser::get_min_tensor_shape(const TensorInfoVector &infos_vec) {
    assert(!infos_vec.empty() && "Tensors info vector cannot be empty");

    // Iterator to min element in containter
    auto min_it =
        std::min_element(infos_vec.begin(), infos_vec.end(),
                         [](const TensorInfo &lhs, const TensorInfo &rhs) { return lhs.size() < rhs.size(); });

    return *min_it;
}

std::pair<size_t, size_t> YoloParser::get_cells_indexes(Layout layout) {
    switch (layout) {
    case Layout::NBCyCx:
        return {3, 2};
    case Layout::NCyCxB:
    case Layout::BCyCx:
        return {2, 1};
    case Layout::CyCxB:
        return {1, 0};
    case Layout::Other:
        return {0, 0};

    default:
        throw std::runtime_error("Unsupported layout");
    }
}

YoloParser::MaskMap YoloParser::masks_to_masks_map(const std::vector<int> &masks_flat, size_t cells_number_min,
                                                   size_t bbox_number_on_cell) {
    MaskMap masks;
    std::vector<size_t> one_side_mask;
    one_side_mask.reserve(bbox_number_on_cell);

    for (size_t i = 0; i < masks_flat.size(); ++i) {
        if (i != 0 && i % bbox_number_on_cell == 0) {
            masks.insert({cells_number_min, one_side_mask});
            one_side_mask.clear();

            cells_number_min *= 2;
        }
        one_side_mask.push_back(masks_flat[i]);
    }

    masks.insert({cells_number_min, one_side_mask});

    return masks;
}

std::vector<DetectionMetadata> YoloParser::parse(const Tensor &tensor) const {
    const float *blob = tensor.data<float>();
    if (!blob)
        throw std::runtime_error("Couldn't get raw tensor data");

    return parse_blob(blob, tensor.info());
}

std::vector<DetectionMetadata> YoloParser::parse_blob(const float *blob, const TensorInfo &blob_info) const {
    assert(blob && "Blob cannot be nullptr");

    size_t side_w = _cells_number_x;
    size_t side_h = _cells_number_y;

    if (_out_shape_layout != Layout::Other) {
        side_w = blob_info.shape[_index_cells_x];
        side_h = blob_info.shape[_index_cells_y];
    } else {
        const size_t blob_size = blob_info.size();
        size_t multiplier = blob_size / (_cells_number_x * _cells_number_y * _num_bbox_on_cell * (5 + _num_classes));
        multiplier = static_cast<size_t>(std::sqrt(static_cast<double>(multiplier)) + 0.5);

        side_w *= multiplier;
        side_h *= multiplier;
    }

    const std::vector<size_t> &mask = _masks.at(std::min(side_w, side_h));

    const size_t side_square = side_w * side_h;

    std::vector<DetectionMetadata> objects;

    for (size_t i = 0; i < side_square; ++i) {

        const size_t row = i / side_w;
        const size_t col = i % side_w;
        for (size_t bbox_cell_num = 0; bbox_cell_num < _num_bbox_on_cell; ++bbox_cell_num) {
            const size_t common_offset = bbox_cell_num * side_square + i;
            const size_t bbox_conf_index = entry_index(side_square, common_offset, NUM_COORDS);
            const size_t bbox_index = entry_index(side_square, common_offset, 0);

            float bbox_conf = blob[bbox_conf_index];
            if (_output_sigmoid_activation)
                bbox_conf = sigmoid(bbox_conf);
            if (bbox_conf < _confidence_threshold)
                continue;

            std::pair<size_t, float> bbox_class = std::make_pair(0, 0.f);
            if (_use_softmax) {
                const auto cls_confs = softmax(blob, _num_classes, common_offset, side_square);

                for (size_t bbox_class_id = 0; bbox_class_id < _num_classes; ++bbox_class_id) {
                    const float bbox_class_prob = cls_confs[bbox_class_id];

                    if (bbox_class_prob > 1.f || bbox_class_prob < 0.f) {
                        printf("bbox_class_prob %f.is out of range [0,1].", bbox_class_prob);
                    }
                    if (bbox_class_prob > bbox_class.second) {
                        bbox_class.first = bbox_class_id;
                        bbox_class.second = bbox_class_prob;
                    }
                }
            } else {
                for (size_t bbox_class_id = 0; bbox_class_id < _num_classes; ++bbox_class_id) {
                    const size_t class_index = entry_index(side_square, common_offset, 5 + bbox_class_id);
                    const float bbox_class_prob = blob[class_index];

                    if (bbox_class_prob > 1.f || bbox_class_prob < 0.f) {
                        printf("bbox_class_prob %f.is out of range [0,1].", bbox_class_prob);
                    }
                    if (bbox_class_prob > bbox_class.second) {
                        bbox_class.first = bbox_class_id;
                        bbox_class.second = bbox_class_prob;
                    }
                }
            }

            const float confidence = bbox_conf * bbox_class.second;
            if (confidence > 1.f || confidence < 0.f) {
                printf("confidence %f.is out of range [0,1].", confidence);
            }
            if (confidence < _confidence_threshold)
                continue;

            // TODO: check if index in array range
            const float raw_x = blob[bbox_index + 0 * side_square];
            const float raw_y = blob[bbox_index + 1 * side_square];
            const float raw_w = blob[bbox_index + 2 * side_square];
            const float raw_h = blob[bbox_index + 3 * side_square];

            auto [x, y, w, h] =
                calc_bounding_box(col, row, raw_x, raw_y, raw_w, raw_h, side_w, side_h, mask[0], bbox_cell_num);

            DetectionMetadata meta(std::make_shared<BaseDictionary>());
            meta.init(x, y, x + w, y + h, confidence, bbox_class.first);
            objects.emplace_back(std::move(meta));
        }
    }

    return objects;
}

std::tuple<double, double, double, double> YoloParser::calc_bounding_box(size_t col, size_t row, float raw_x,
                                                                         float raw_y, float raw_w, float raw_h,
                                                                         size_t side_w, size_t side_h, size_t mask_0,
                                                                         size_t bbox_cell_num) const {

    if (_output_sigmoid_activation) {
        raw_x = sigmoid(raw_x);
        raw_y = sigmoid(raw_y);
    }

    float x = static_cast<float>(col + raw_x) / side_w * _image_width;
    float y = static_cast<float>(row + raw_y) / side_h * _image_height;

    const size_t anchor_index = (2 * mask_0) + (2 * bbox_cell_num);
    if (_anchors.size() <= anchor_index + 1)
        throw std::runtime_error("Invalid anchor index: out of array bounds");
    float w = std::exp(raw_w) * _anchors[anchor_index];
    float h = std::exp(raw_h) * _anchors[anchor_index + 1];

    convert_to_relative_coords(x, y, w, h);
    return {x, y, w, h};
}

void YoloParser::convert_to_relative_coords(float &x_min, float &y_min, float &w, float &h) const {
    x_min -= w / 2;
    y_min -= h / 2;
    x_min /= _image_width;
    w /= _image_width;
    y_min /= _image_height;
    h /= _image_height;
}

size_t YoloParser::entry_index(size_t side_square, size_t location, size_t entry) const noexcept {
    size_t bbox_cell_num = location / side_square;
    size_t loc = location % side_square;
    // side_square is the tensor dimension of the YoloV3 model. Overflow is not possible here.
    return side_square * (bbox_cell_num * (_num_classes + 5) + entry) + loc;
}

std::vector<float> YoloParser::softmax(const float *arr, size_t size, size_t common_offset, size_t side_square) const {
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        const size_t class_index = entry_index(side_square, common_offset, 5 + i);
        sftm_arr[i] = std::exp(arr[class_index]);
        sum += sftm_arr[i];
    }
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] /= sum;
    }
    return sftm_arr;
}

std::tuple<double, double, double, double> Yolo5Parser::calc_bounding_box(size_t col, size_t row, float raw_x,
                                                                          float raw_y, float raw_w, float raw_h,
                                                                          size_t side_w, size_t side_h, size_t mask_0,
                                                                          size_t bbox_cell_num) const {

    if (_output_sigmoid_activation) {
        raw_x = sigmoid(raw_x);
        raw_y = sigmoid(raw_y);
    }

    float x = static_cast<float>(col + 2 * raw_x - 0.5) / side_w * _image_width;
    float y = static_cast<float>(row + 2 * raw_y - 0.5) / side_h * _image_height;

    const size_t anchor_index = (2 * mask_0) + (2 * bbox_cell_num);
    if (_anchors.size() <= anchor_index + 1)
        throw std::runtime_error("Invalid anchor index: out of array bounds");
    float width = std::pow(sigmoid(raw_w) * 2, 2) * _anchors[anchor_index];
    float height = std::pow(sigmoid(raw_h) * 2, 2) * _anchors[anchor_index + 1];

    convert_to_relative_coords(x, y, width, height);
    return {x, y, width, height};
}

} // namespace dlstreamer