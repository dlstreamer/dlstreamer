/*******************************************************************************
 * Copyright (C) 2021-2024 Intel Corporation
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

const double DEFAULT_IOU_THRESHOLD = 0.4;

class BlobToROIConverter : public BlobToMetaConverter {
  protected:
    struct DetectedObject {
        double x;
        double y;
        double w;
        double h;
        double r;

        double confidence;

        size_t label_id;
        std::string label;

        const float *mask_data;
        size_t mask_width;
        size_t mask_height;

        DetectedObject(double x, double y, double w, double h, double r, double confidence, size_t label_id,
                       const std::string &label, double w_scale = 1.f, double h_scale = 1.f,
                       bool relative_to_center = false, const float *mask_data = nullptr, size_t mask_width = 0,
                       size_t mask_height = 0)
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

            this->r = r;

            this->mask_data = mask_data;
            this->mask_width = mask_width;
            this->mask_height = mask_height;
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
                              G_TYPE_DOUBLE, y, "y_max", G_TYPE_DOUBLE, y + h, "radius", G_TYPE_DOUBLE, r, NULL);

            if (not label.empty())
                gst_structure_set(detection_tensor, "label", G_TYPE_STRING, label.c_str(), NULL);

            if (mask_data != nullptr) {

                size_t length = mask_height * mask_width;
                GValueArray *g_arr = g_value_array_new(length);
                if (not g_arr)
                    throw std::runtime_error("Failed to create GValueArray with " + std::to_string(length) +
                                             " elements");

                try {
                    GValue gvalue = G_VALUE_INIT;
                    g_value_init(&gvalue, G_TYPE_FLOAT);
                    for (guint i = 0; i < length; ++i) {
                        g_value_set_float(&gvalue, mask_data[i]);
                        g_value_array_append(g_arr, &gvalue);
                    }
                } catch (const std::exception &e) {
                    if (g_arr)
                        g_value_array_free(g_arr);
                    std::throw_with_nested(std::runtime_error("Failed to convert mask_data to GValueArray"));
                }

                gst_structure_set(detection_tensor, "mask_width", G_TYPE_INT, mask_width, "mask_height", G_TYPE_INT,
                                  mask_height, NULL);
                gst_structure_set_array(detection_tensor, "mask", g_arr);
            }

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

    BlobToROIConverter(BlobToMetaConverter::Initializer initializer, double confidence_threshold, bool need_nms,
                       double iou_threshold)
        : BlobToMetaConverter(std::move(initializer)), confidence_threshold(confidence_threshold), need_nms(need_nms),
          iou_threshold(iou_threshold) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;

    static BlobToMetaConverter::Ptr create(BlobToMetaConverter::Initializer initializer,
                                           const std::string &converter_name);
    static const size_t min_dims_size = 2;
};

} // namespace post_processing
