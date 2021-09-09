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

class YOLOv3Converter : public YOLOBaseConverter {
  protected:
    const std::map<size_t, std::vector<size_t>> masks;
    const size_t coords = 4;
    const size_t input_size;

    size_t entryIndex(size_t side, size_t location, size_t entry) const;
    std::vector<float> softmax(const float *arr, size_t size, size_t common_offset, size_t side) const;

    void parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &blob, std::vector<DetectedObject> &objects) const;

  public:
    YOLOv3Converter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                    GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                    double confidence_threshold, size_t classes_number, std::vector<float> anchors,
                    std::map<size_t, std::vector<size_t>> masks, size_t cells_number_x, size_t cells_number_y,
                    double iou_threshold, size_t bbox_number_on_cell, size_t input_size, bool do_cls_softmax,
                    bool output_sigmoid_activation)
        : YOLOBaseConverter(model_name, input_image_info, std::move(model_proc_output_info), labels,
                            confidence_threshold, anchors, iou_threshold,
                            {classes_number, cells_number_x, cells_number_y, bbox_number_on_cell}, do_cls_softmax,
                            output_sigmoid_activation),
          masks(masks), input_size(input_size) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "tensor_to_bbox_yolo_v3";
    }
};
} // namespace post_processing
