/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
 * Copyright (C) 2024-2025 Videonetics Technology Pvt Ltd
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "yolo_vx.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "opencv4/opencv2/core.hpp"
#include "safe_arithmetic.hpp"
#include <gst/gst.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace post_processing;
namespace {

size_t getClassesNum(GstStructure *s, size_t labels_num) {
    if (!gst_structure_has_field(s, "classes") && labels_num)
        return labels_num;

    int _classes = 0;
    gst_structure_get_int(s, "classes", &_classes);

    size_t classes = safe_convert<size_t>(_classes);

    if (!labels_num)
        return classes;

    if (classes < labels_num)
        GST_WARNING("Number of classes does not match with namber of labels: classes=%lu, labels=%lu.", classes,
                    labels_num);

    if (classes > labels_num)
        throw std::runtime_error("Number of classes greater then number of labels.");

    return classes;
}

double getIOUThreshold(GstStructure *s) {
    double iou_threshold = 0.5;
    if (gst_structure_has_field(s, "iou_threshold")) {
        gst_structure_get_double(s, "iou_threshold", &iou_threshold);
    }

    return iou_threshold;
}
} // namespace

BlobToMetaConverter::Ptr YOLOvxConverter::create(BlobToMetaConverter::Initializer initializer,
                                                 const std::string &converter_name, double confidence_threshold) {
    try {
        GstStructure *model_proc_output_info = initializer.model_proc_output_info.get();
        const auto classes_number = getClassesNum(model_proc_output_info, initializer.labels.size());
        if (!classes_number)
            throw std::runtime_error("Number of classes if null.");
        const auto iou_threshold = getIOUThreshold(model_proc_output_info);
        std::vector<int> strides = {8, 16, 32};
        std::vector<GridAndStride> grid_strides;
        auto input_w = initializer.input_image_info.width;
        auto input_h = initializer.input_image_info.height;
        for (auto stride : strides) {
            int num_grid_w = input_w / stride;
            int num_grid_h = input_h / stride;
            for (int g1 = 0; g1 < num_grid_h; g1++) {
                for (int g0 = 0; g0 < num_grid_w; g0++) {
                    grid_strides.push_back((GridAndStride){g0, g1, stride});
                }
            }
        }
        return BlobToMetaConverter::Ptr(new YOLOvxConverter(std::move(initializer), confidence_threshold, true,
                                                            iou_threshold, classes_number, strides, grid_strides));
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create \"" + converter_name + "\" converter."));
    }
    return nullptr;
}

YOLOvxConverter::YOLOvxConverter(BlobToMetaConverter::Initializer initializer, double confidence_threshold,
                                 bool need_nms, double iou_threshold, const size_t classes_number,
                                 const std::vector<int> &strides, const std::vector<GridAndStride> &grid_strides)
    : classes_number(classes_number), strides(strides), grid_strides(grid_strides),
      BlobToROIConverter(std::move(initializer), confidence_threshold, need_nms, iou_threshold) {
}

void YOLOvxConverter::generateYoloxProposals(const float *blob_data, const std::vector<size_t> &blob_dims,
                                             size_t blob_size, std::vector<DetectedObject> &objects) const {
    auto input_w = getModelInputImageInfo().width;
    auto input_h = getModelInputImageInfo().height;
    const int num_anchors = grid_strides.size();
    const double prob_threshold = confidence_threshold;
    for (int anchor_idx = 0; anchor_idx < num_anchors; anchor_idx++) {
        const int grid0 = grid_strides[anchor_idx].grid0;
        const int grid1 = grid_strides[anchor_idx].grid1;
        const int stride = grid_strides[anchor_idx].stride;

        const int basic_pos = anchor_idx * (classes_number + 5);

        // yolox/models/yolo_head.py decode logic
        //  outputs[..., :2] = (outputs[..., :2] + grids) * strides
        //  outputs[..., 2:4] = torch.exp(outputs[..., 2:4]) * strides
        double x_center = (blob_data[basic_pos + 0] + grid0) * stride;
        double y_center = (blob_data[basic_pos + 1] + grid1) * stride;
        double w = exp(blob_data[basic_pos + 2]) * stride;
        double h = exp(blob_data[basic_pos + 3]) * stride;
        double x0 = x_center - w * 0.5f;
        double y0 = y_center - h * 0.5f;

        double box_objectness = blob_data[basic_pos + 4];
        for (int class_idx = 0; class_idx < classes_number; class_idx++) {
            double box_cls_score = blob_data[basic_pos + 5 + class_idx];
            double box_prob = box_objectness * box_cls_score;
            if (box_prob > prob_threshold) {
                DetectedObject obj(x0 / input_w, y0 / input_h, w / input_w, h / input_h, box_prob, class_idx,
                                   getLabelByLabelId(class_idx));
                objects.push_back(obj);
            }
        } // class loop
    } // point anchor loop
}

void YOLOvxConverter::parseOutputBlob(const float *blob_data, const std::vector<size_t> &blob_dims, size_t blob_size,
                                      std::vector<DetectedObject> &objects) const {
    if (!blob_data)
        throw std::invalid_argument("Output blob data is nullptr.");

    generateYoloxProposals(blob_data, blob_dims, blob_size, objects);
}

TensorsTable YOLOvxConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    try {
        const auto &model_input_image_info = getModelInputImageInfo();
        size_t batch_size = model_input_image_info.batch_size;
        DetectedObjectsTable objects_table(batch_size);
        for (size_t batch_number = 0; batch_number < batch_size; ++batch_number) {
            auto &objects = objects_table[batch_number];

            for (const auto &blob_iter : output_blobs) {
                const InferenceBackend::OutputBlob::Ptr &blob = blob_iter.second;
                if (!blob)
                    throw std::invalid_argument("Output blob is nullptr.");

                if (!blob->GetData())
                    throw std::runtime_error("Output blob data is nullptr.");

                if (blob->GetPrecision() != InferenceBackend::Blob::Precision::FP32)
                    throw std::runtime_error("Unsupported label precision.");

                size_t unbatched_size = blob->GetSize() / batch_size;
                parseOutputBlob(reinterpret_cast<const float *>(blob->GetData()) + unbatched_size * batch_number,
                                blob->GetDims(), unbatched_size, objects);
            }
        }

        return storeObjects(objects_table);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do YoloVX post-processing."));
    }
    return TensorsTable{};
}