/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "converters/yolo_base.h"

namespace DetectionPlugin {
namespace Converters {

class YOLOV2Converter : public YOLOConverter {
  protected:
    struct OutputLayerShapeConfig {
        const size_t classes_number;
        const size_t cells_number_x;
        const size_t cells_number_y;
        const size_t bbox_number_on_cell;

        const size_t one_bbox_blob_size;
        const size_t common_cells_number;
        const size_t one_scale_bboxes_blob_size;
        const size_t requied_blob_size;

        enum Index : size_t { X = 0, Y = 1, W = 2, H = 3, CONFIDENCE = 4, FIRST_CLASS_PROB = 5 };
        OutputLayerShapeConfig() = delete;
        OutputLayerShapeConfig(size_t classes_number, size_t cells_number_x, size_t cells_number_y,
                               size_t bbox_number_on_cell)
            : classes_number(classes_number), cells_number_x(cells_number_x), cells_number_y(cells_number_y),
              bbox_number_on_cell(bbox_number_on_cell),
              one_bbox_blob_size(classes_number + 5), // classes prob + x, y, w, h, confidence
              common_cells_number(cells_number_x * cells_number_y),
              one_scale_bboxes_blob_size(one_bbox_blob_size * common_cells_number),
              requied_blob_size(one_scale_bboxes_blob_size * bbox_number_on_cell) {
        }
    };
    OutputLayerShapeConfig output_shape_info;

    size_t getIndex(size_t index, size_t offset) const;
    size_t getIndex(size_t index, size_t k, size_t i, size_t j) const;

  public:
    YOLOV2Converter(size_t classes_number, std::vector<float> anchors, size_t cells_number_x, size_t cells_number_y,
                    double iou_threshold = 0.5, size_t bbox_number_on_cell = 5);

    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                 double confidence_threshold, GValueArray *labels);
};

} // namespace Converters
} // namespace DetectionPlugin
