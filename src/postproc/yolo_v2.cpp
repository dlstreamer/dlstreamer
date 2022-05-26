/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_base.h"
#include <cmath>

namespace dlstreamer {

class PostProcYoloV2 : public PostProcYolo {
  public:
    PostProcYoloV2(ITransformController &transform_ctrl, DictionaryCPtr params)
        : PostProcYolo(transform_ctrl, std::move(params)) {
    }
    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        // call base class set_info()
        PostProcYolo::set_info(input_info, output_info);

        if (_info.planes.size() != 1)
            throw std::runtime_error("Yolo v2 converter can process models with only one output.");
        const auto &blob_dims = _info.planes[0].shape;

        if (_dims_layout != OutputDimsLayout::NO) {
            size_t cells_x_i = 0;
            size_t cells_y_i = 0;
            switch (_dims_layout) {
            case OutputDimsLayout::NBCxCy:
                cells_x_i = 2;
                cells_y_i = 3;
                break;
            case OutputDimsLayout::NCxCyB:
                cells_x_i = 1;
                cells_y_i = 2;
                break;
            case OutputDimsLayout::BCxCy:
                cells_x_i = 1;
                cells_y_i = 2;
                break;
            case OutputDimsLayout::CxCyB:
                cells_x_i = 0;
                cells_y_i = 1;
                break;

            default:
                break;
            }

            if (static_cast<size_t>(_cells_number_x) != blob_dims[cells_x_i]) {
                throw std::runtime_error(
                    "Mismatch between cells_number_x: " + std::to_string(_cells_number_x) +
                    " - and the actual of the bounding box: " + std::to_string(blob_dims[cells_x_i]));
            }
            if (static_cast<size_t>(_cells_number_y) != blob_dims[cells_y_i]) {
                throw std::runtime_error(
                    "Mismatch between cells_number_y: " + std::to_string(_cells_number_y) +
                    " - and the actual of the bounding box: " + std::to_string(blob_dims[cells_y_i]));
            }
        }

        size_t batch_size = 1; // TODO input_info.batch_size;

        size_t blob_size = std::accumulate(blob_dims.cbegin(), blob_dims.cend(), 1lu, std::multiplies<size_t>());
        size_t required_blob_size =
            batch_size * _cells_number_x * _cells_number_y * _bbox_number_on_cell * (_classes_number + 5);

        if (blob_size != required_blob_size) {
            throw std::runtime_error("Size of the resulting output blob " + std::to_string(blob_size) +
                                     " does not match the required " + std::to_string(required_blob_size));
        }
    }

    bool process(BufferPtr src) override {
        auto src_cpu = _input_mapper->map(src, AccessMode::READ);
        float *blob_data = static_cast<float *>(src_cpu->data());
        auto dims = src->info()->planes[0].shape;

        if (!blob_data)
            throw std::invalid_argument("Output blob data is nullptr");

        int one_bbox_blob_size = _classes_number + 5; // classes prob + x, y, w, h, confidence
        int common_cells_number = _cells_number_x * _cells_number_y;
        int one_scale_bboxes_blob_size = one_bbox_blob_size * common_cells_number;

        std::vector<DetectionMetadata> candidates;

        for (int bbox_scale_index = 0; bbox_scale_index < _bbox_number_on_cell; ++bbox_scale_index) {
            const float anchor_scale_w = _anchors[bbox_scale_index * 2];
            const float anchor_scale_h = _anchors[bbox_scale_index * 2 + 1];

            for (int cell_index_x = 0; cell_index_x < _cells_number_x; ++cell_index_x) {
                for (int cell_index_y = 0; cell_index_y < _cells_number_y; ++cell_index_y) {

                    const size_t common_offset =
                        bbox_scale_index * one_scale_bboxes_blob_size + cell_index_y * _cells_number_y + cell_index_x;

                    float bbox_confidence = blob_data[getIndex(Index::CONFIDENCE, common_offset)];
                    if (_output_sigmoid_activation)
                        bbox_confidence = sigmoid(bbox_confidence);
                    if (bbox_confidence <= _confidence_threshold)
                        continue;

                    std::pair<size_t, float> bbox_class = std::make_pair(0, 0.f);
                    if (_do_cls_softmax) {
                        const auto cls_confs = softmax(blob_data, _classes_number, common_offset);

                        for (int bbox_class_id = 0; bbox_class_id < _classes_number; ++bbox_class_id) {
                            const float bbox_class_prob = cls_confs[bbox_class_id];

                            if (bbox_class_prob > bbox_class.second) {
                                bbox_class.first = bbox_class_id;
                                bbox_class.second = bbox_class_prob;
                            }
                        }
                    } else {
                        for (int bbox_class_id = 0; bbox_class_id < _classes_number; ++bbox_class_id) {
                            const float bbox_class_prob =
                                blob_data[getIndex((Index::FIRST_CLASS_PROB + bbox_class_id), common_offset)];

                            if (bbox_class_prob > bbox_class.second) {
                                bbox_class.first = bbox_class_id;
                                bbox_class.second = bbox_class_prob;
                            }
                        }
                    }

                    bbox_confidence *= bbox_class.second;
                    if (bbox_confidence <= _confidence_threshold)
                        continue;

                    const float raw_x = blob_data[getIndex(Index::X, common_offset)];
                    const float raw_y = blob_data[getIndex(Index::Y, common_offset)];
                    const float raw_w = blob_data[getIndex(Index::W, common_offset)];
                    const float raw_h = blob_data[getIndex(Index::H, common_offset)];

                    // scale back to image width/height
                    float bbox_x =
                        (cell_index_x + (_output_sigmoid_activation ? sigmoid(raw_x) : raw_x)) / _cells_number_x;
                    float bbox_y =
                        (cell_index_y + (_output_sigmoid_activation ? sigmoid(raw_y) : raw_y)) / _cells_number_y;
                    float bbox_w = (std::exp(raw_w) * anchor_scale_w) / _cells_number_x;
                    float bbox_h = (std::exp(raw_h) * anchor_scale_h) / _cells_number_y;

                    // relative_to_center
                    bbox_x -= bbox_w * 0.5f;
                    bbox_y -= bbox_h * 0.5f;

                    DetectionMetadata meta;
                    meta.init(bbox_x, bbox_y, bbox_x + bbox_w, bbox_y + bbox_h, bbox_confidence, bbox_class.first,
                              _labels[bbox_class.first]);
                    candidates.push_back(meta);
                }
            }
        }

        runNms(candidates);

        for (auto bbox : candidates) {
            auto meta = DetectionMetadata(src->add_metadata("detection"));
            meta.init(bbox.x_min(), bbox.y_min(), bbox.x_max(), bbox.y_max(), bbox.confidence(), bbox.label_id(),
                      bbox.label());
        }

        return true;
    }

    size_t getIndex(size_t index, size_t offset) const {
        return index * _cells_number_x * _cells_number_y + offset;
    }

    std::vector<float> softmax(const float *arr, size_t size, size_t common_offset) {
        std::vector<float> sftm_arr(size);
        float sum = 0;
        for (size_t i = 0; i < size; ++i) {
            sftm_arr[i] = std::exp(arr[getIndex((Index::FIRST_CLASS_PROB + i), common_offset)]);
            sum += sftm_arr[i];
        }
        for (size_t i = 0; i < size; ++i) {
            sftm_arr[i] /= sum;
        }
        return sftm_arr;
    }

    static float sigmoid(float x) {
        return 1 / (1 + std::exp(-x));
    };
};

TransformDesc PostProcYoloV2Desc = {.name = "tensor_postproc_yolo_v2",
                                    .description = "Post-processing of YoloV2 model to extract bounding box list",
                                    .author = "Intel Corporation",
                                    .params = &params_desc,
                                    .input_info = {MediaType::TENSORS},
                                    .output_info = {MediaType::TENSORS},
                                    .create = TransformBase::create<PostProcYoloV2>,
                                    .flags = TRANSFORM_FLAG_SUPPORT_PARAMS_STRUCTURE};

} // namespace dlstreamer
