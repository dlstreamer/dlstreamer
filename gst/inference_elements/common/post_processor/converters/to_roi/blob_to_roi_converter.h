/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "post_processor/blob_to_meta_converter.h"
#include "post_processor/post_proc_common.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>

namespace post_processing {

class BlobToROIConverter : public BlobToMetaConverter {
  protected:
    struct DetectedObject {
        double x;
        double y;
        double w;
        double h;

        double confidence;

        size_t label_id;
        std::string label;

        DetectedObject(double x, double y, double w, double h, double confidence, size_t label_id,
                       const std::string &label, double h_scale = 1.f, double w_scale = 1.f,
                       bool relative_to_center = false)
            : confidence(confidence), label_id(label_id), label(label) {
            if (relative_to_center) {
                this->x = (x - w / 2) * w_scale;
                this->y = (y - h / 2) * h_scale;
            } else {
                this->x = x * w_scale;
                this->y = y * h_scale;
            }

            this->w = w * w_scale;
            this->h = h * h_scale;
        }

        bool operator<(const DetectedObject &other) const {
            return this->confidence < other.confidence;
        }

        bool operator>(const DetectedObject &other) const {
            return this->confidence > other.confidence;
        }

        GstStructure *toTensor(const GstStructureUniquePtr &detection_result) const {
            GstStructure *detection_tensor = gst_structure_copy(detection_result.get());

            gst_structure_set_name(detection_tensor, "detection"); // make sure name="detection"
            gst_structure_set(detection_tensor, "label_id", G_TYPE_INT, label_id, "confidence", G_TYPE_DOUBLE,
                              confidence, "x_min", G_TYPE_DOUBLE, x, "x_max", G_TYPE_DOUBLE, x + w, "y_min",
                              G_TYPE_DOUBLE, y, "y_max", G_TYPE_DOUBLE, y + h, NULL);

            if (not label.empty())
                gst_structure_set(detection_tensor, "label", G_TYPE_STRING, label.c_str(), NULL);

            return detection_tensor;
        }
    };
    using DetectedObjectsTable = std::vector<std::vector<DetectedObject>>;

    TensorsTable storeObjects(DetectedObjectsTable &objects) const;
    void runNms(std::vector<DetectedObject> &candidates) const;
    TensorsTable toTensorsTable(const DetectedObjectsTable &bboxes_table) const;

    const double confidence_threshold;
    const bool need_nms;
    const double iou_threshold;

  public:
    BlobToROIConverter() = delete;

    BlobToROIConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                       GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                       double confidence_threshold, bool need_nms, double iou_threshold)
        : BlobToMetaConverter(model_name, input_image_info, std::move(model_proc_output_info), labels),
          confidence_threshold(confidence_threshold), need_nms(need_nms), iou_threshold(iou_threshold) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;

    static BlobToMetaConverter::Ptr create(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                                           GstStructureUniquePtr model_proc_output_info,
                                           const std::vector<std::string> &labels, const std::string &converter_name);
    static const size_t min_dims_size = 2;
};

} // namespace post_processing
