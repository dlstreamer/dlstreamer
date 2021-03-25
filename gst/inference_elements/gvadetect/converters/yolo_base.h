/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "converters/converter.h"

#include <cmath>

namespace DetectionPlugin {
namespace Converters {

class YOLOConverter : public Converter {
  protected:
    const std::vector<float> anchors;
    const double iou_threshold;
    struct DetectedObject {
        gfloat x;
        gfloat y;
        gfloat w;
        gfloat h;
        guint class_id;
        gfloat confidence;

        DetectedObject(gfloat x, gfloat y, gfloat w, gfloat h, guint class_id, gfloat confidence, gfloat h_scale = 1.f,
                       gfloat w_scale = 1.f) {
            this->x = (x - w / 2) * w_scale;
            this->y = (y - h / 2) * h_scale;
            this->w = w * w_scale;
            this->h = h * h_scale;
            this->class_id = class_id;
            this->confidence = confidence;
        }

        bool operator<(const DetectedObject &other) const {
            return this->confidence < other.confidence;
        }

        bool operator>(const DetectedObject &other) const {
            return this->confidence > other.confidence;
        }
    };

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

    inline float sigmoid(float x) {
        return 1 / (1 + std::exp(-x));
    }

    void runNms(std::vector<DetectedObject> &candidates);
    void storeObjects(std::vector<DetectedObject> &objects, const std::shared_ptr<InferenceFrame> frame,
                      GstStructure *detection_result, GValueArray *labels);

  public:
    YOLOConverter() = delete;
    YOLOConverter(std::vector<float> anchors, double iou_threshold, OutputLayerShapeConfig output_shape_info,
                  bool do_cls_softmax, bool output_sigmoid_activation)
        : anchors(anchors), iou_threshold(iou_threshold), output_shape_info(output_shape_info),
          do_cls_softmax(do_cls_softmax), output_sigmoid_activation(output_sigmoid_activation) {
    }
    virtual ~YOLOConverter() = default;
    virtual bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                         double confidence_threshold, GValueArray *labels) = 0;

    static YOLOConverter *makeYOLOConverter(const std::string &converter_type, const GstStructure *model_proc_info,
                                            const ModelInputInfo &input_info);
};
} // namespace Converters
} // namespace DetectionPlugin
