/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class YOLOBaseConverter : public BlobToROIConverter {
  protected:
    const std::vector<float> anchors;

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

    const OutputLayerShapeConfig output_shape_info;
    const bool do_cls_softmax;
    const bool output_sigmoid_activation;

    inline float sigmoid(float x) const {
        return 1 / (1 + std::exp(-x));
    }

  public:
    YOLOBaseConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                      GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                      double confidence_threshold, std::vector<float> anchors, double iou_threshold,
                      OutputLayerShapeConfig output_shape_info, bool do_cls_softmax, bool output_sigmoid_activation)
        : BlobToROIConverter(model_name, input_image_info, std::move(model_proc_output_info), labels,
                             confidence_threshold, true, iou_threshold),
          anchors(anchors), output_shape_info(output_shape_info), do_cls_softmax(do_cls_softmax),
          output_sigmoid_activation(output_sigmoid_activation) {
    }
    virtual ~YOLOBaseConverter() = default;

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;

    static BlobToMetaConverter::Ptr create(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                                           GstStructureUniquePtr model_proc_output_info,
                                           const std::vector<std::string> &labels, const std::string &converter_name,
                                           double confidence_thresholds);
};
} // namespace post_processing
