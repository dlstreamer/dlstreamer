/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "yolo_base.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class YOLOv2Converter : public YOLOBaseConverter {
  protected:
    size_t getIndex(size_t index, size_t offset) const;
    size_t getIndex(size_t index, size_t k, size_t i, size_t j) const;
    std::vector<float> softmax(const float *arr, size_t size, size_t common_offset) const;

    void parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<DetectedObject> &objects) const;

  public:
    YOLOv2Converter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                    GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                    double confidence_threshold, size_t classes_number, std::vector<float> anchors,
                    size_t cells_number_x, size_t cells_number_y, double iou_threshold, size_t bbox_number_on_cell,
                    bool do_cls_softmax, bool output_sigmoid_activation)
        : YOLOBaseConverter(model_name, input_image_info, std::move(model_proc_output_info), labels,
                            confidence_threshold, anchors, iou_threshold,
                            {classes_number, cells_number_x, cells_number_y, bbox_number_on_cell}, do_cls_softmax,
                            output_sigmoid_activation) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "tensor_to_bbox_yolo_v2";
    }
};
} // namespace post_processing
