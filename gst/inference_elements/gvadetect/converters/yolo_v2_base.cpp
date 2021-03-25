/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/yolo_v2_base.h"

#include "inference_backend/logger.h"

using namespace DetectionPlugin;
using namespace Converters;

YOLOV2Converter::YOLOV2Converter(size_t classes_number, std::vector<float> anchors, size_t cells_number_x,
                                 size_t cells_number_y, double iou_threshold, size_t bbox_number_on_cell,
                                 bool do_cls_softmax, bool output_sigmoid_activation)
    : YOLOConverter(anchors, iou_threshold, {classes_number, cells_number_x, cells_number_y, bbox_number_on_cell},
                    do_cls_softmax, output_sigmoid_activation) {
}

size_t YOLOV2Converter::getIndex(size_t index, size_t offset) const {
    return index * output_shape_info.common_cells_number + offset;
}

std::vector<float> YOLOV2Converter::softmax(const float *arr, size_t common_offset, size_t size) {
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] = std::exp(arr[getIndex((OutputLayerShapeConfig::Index::FIRST_CLASS_PROB + i), common_offset)]);
        sum += sftm_arr[i];
    }
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] /= sum;
    }
    return sftm_arr;
}

bool YOLOV2Converter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                              const std::vector<std::shared_ptr<InferenceFrame>> &frames,
                              GstStructure *detection_result, double confidence_threshold, GValueArray *labels) {
    ITT_TASK(__FUNCTION__);

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

        const float *blob_data = (const float *)blob->GetData();
        if (not blob_data)
            throw std::invalid_argument("Output blob data is nullptr");

        const auto blob_dims = blob->GetDims();
        size_t blob_size = 1;
        for (const auto &dim : blob_dims)
            blob_size *= dim;

        if (blob_size != output_shape_info.requied_blob_size)
            throw std::runtime_error("Size of the resulting output blob (" + std::to_string(blob_size) +
                                     ") does not match the required (" +
                                     std::to_string(output_shape_info.requied_blob_size) + ").");

        for (size_t bbox_scale_index = 0; bbox_scale_index < output_shape_info.bbox_number_on_cell;
             ++bbox_scale_index) {
            const float anchor_scale_w = anchors[bbox_scale_index * 2];
            const float anchor_scale_h = anchors[bbox_scale_index * 2 + 1];

            for (size_t cell_index_x = 0; cell_index_x < output_shape_info.cells_number_x; ++cell_index_x) {
                for (size_t cell_index_y = 0; cell_index_y < output_shape_info.cells_number_y; ++cell_index_y) {

                    const size_t common_offset = bbox_scale_index * output_shape_info.one_scale_bboxes_blob_size +
                                                 cell_index_y * output_shape_info.cells_number_y + cell_index_x;

                    using Index = YOLOV2Converter::OutputLayerShapeConfig::Index;

                    float bbox_confidence = blob_data[getIndex(Index::CONFIDENCE, common_offset)];
                    if (output_sigmoid_activation)
                        bbox_confidence = sigmoid(bbox_confidence);
                    if (bbox_confidence <= confidence_threshold)
                        continue;

                    std::pair<size_t, float> bbox_class = std::make_pair(0, 0.f);
                    if (do_cls_softmax) {
                        const auto cls_confs = softmax(blob_data, common_offset, output_shape_info.classes_number);

                        for (size_t bbox_class_id = 0; bbox_class_id < output_shape_info.classes_number;
                             ++bbox_class_id) {
                            const float bbox_class_prob = cls_confs[bbox_class_id];

                            if (bbox_class_prob > 1.f) {
                                GST_WARNING("bbox_class_prob is weird %f", bbox_class_prob);
                            }
                            if (bbox_class_prob > bbox_class.second) {
                                bbox_class.first = bbox_class_id;
                                bbox_class.second = bbox_class_prob;
                            }
                        }
                    } else {
                        for (size_t bbox_class_id = 0; bbox_class_id < output_shape_info.classes_number;
                             ++bbox_class_id) {
                            const float bbox_class_prob =
                                blob_data[getIndex((Index::FIRST_CLASS_PROB + bbox_class_id), common_offset)];

                            if (bbox_class_prob > 1.f) {
                                GST_WARNING("bbox_class_prob is weird %f", bbox_class_prob);
                            }
                            if (bbox_class_prob > bbox_class.second) {
                                bbox_class.first = bbox_class_id;
                                bbox_class.second = bbox_class_prob;
                            }
                        }
                    }

                    bbox_confidence *= bbox_class.second;
                    if (bbox_confidence <= confidence_threshold)
                        continue;

                    const float raw_x = blob_data[getIndex(Index::X, common_offset)];
                    const float raw_y = blob_data[getIndex(Index::Y, common_offset)];
                    const float raw_w = blob_data[getIndex(Index::W, common_offset)];
                    const float raw_h = blob_data[getIndex(Index::H, common_offset)];

                    // scale back to image width/height
                    const float bbox_x = (cell_index_x + (output_sigmoid_activation ? sigmoid(raw_x) : raw_x)) /
                                         output_shape_info.cells_number_x;
                    const float bbox_y = (cell_index_y + (output_sigmoid_activation ? sigmoid(raw_y) : raw_y)) /
                                         output_shape_info.cells_number_y;
                    const float bbox_w = (std::exp(raw_w) * anchor_scale_w) / output_shape_info.cells_number_x;
                    const float bbox_h = (std::exp(raw_h) * anchor_scale_h) / output_shape_info.cells_number_y;

                    DetectedObject object(bbox_x, bbox_y, bbox_w, bbox_h, bbox_class.first, bbox_confidence);
                    objects.push_back(object);
                }
            }
        }
    }
    storeObjects(objects, frames[0], detection_result, labels);
    return true;
}
