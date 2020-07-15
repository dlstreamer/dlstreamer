/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "converters/converter.h"

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

    void runNms(std::vector<DetectedObject> &candidates);
    void storeObjects(std::vector<DetectedObject> &objects, const std::shared_ptr<InferenceFrame> frame,
                      GstStructure *detection_result, GValueArray *labels);

  public:
    YOLOConverter() = delete;
    YOLOConverter(std::vector<float> anchors, double iou_threshold) : anchors(anchors), iou_threshold(iou_threshold) {
    }
    virtual ~YOLOConverter() = default;
    virtual bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                         double confidence_threshold, GValueArray *labels) = 0;

    static YOLOConverter *makeYOLOConverter(const std::string &converter_type, const GstStructure *model_proc_info);
};
} // namespace Converters
} // namespace DetectionPlugin
