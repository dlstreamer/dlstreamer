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
    size_t getIndex(size_t index, size_t offset) const;
    size_t getIndex(size_t index, size_t k, size_t i, size_t j) const;
    float sigmoid(float x);
    std::vector<float> softmax(const float *arr, size_t common_offset, size_t size);

  public:
    YOLOV2Converter(size_t classes_number, std::vector<float> anchors, size_t cells_number_x, size_t cells_number_y,
                    double iou_threshold = 0.5, size_t bbox_number_on_cell = 5, bool do_cls_softmax = false,
                    bool output_sigmoid_activation = false);

    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                 double confidence_threshold, GValueArray *labels);
};

} // namespace Converters
} // namespace DetectionPlugin
