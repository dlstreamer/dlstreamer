/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_tensor_converter.h"
#include <opencv2/imgproc.hpp>
#include <string>

#define DEF_ANOMALY_TENSOR_LAYOUT_OFFSET_BS 0 // batch size
#define DEF_ANOMALY_TENSOR_LAYOUT_OFFSET_CH 1 // single channel (anomaly map)
#define DEF_ANOMALY_TENSOR_LAYOUT_OFFSET_H 2  // image height
#define DEF_ANOMALY_TENSOR_LAYOUT_OFFSET_W 3  // image width
#define DEF_ANOMALY_TENSOR_LAYOUT_SIZE 4      // size of the tensor
#define DEF_TOTAL_LABELS_COUNT 2              // Normal and Anomaly

namespace post_processing {

class DetectionAnomalyConverter : public BlobToTensorConverter {
    double image_threshold;
    double normalization_scale;
    std::string anomaly_detection_task;
    uint lbl_normal_cnt = 0;
    uint lbl_anomaly_cnt = 0;

  protected:
    // Helper function to normalize the thresholds
    double normalize(double &value, float threshold = 0.0) {
        double normalized = ((value - threshold) / normalization_scale) + 0.5f;
        return std::min(std::max(normalized, 0.), 1.);
    }

  public:
    DetectionAnomalyConverter(BlobToMetaConverter::Initializer initializer, double image_threshold,
                              double normalization_scale, std::string anomaly_detection_task = "")
        : BlobToTensorConverter(std::move(initializer)), image_threshold(image_threshold),
          normalization_scale(normalization_scale), anomaly_detection_task(std::move(anomaly_detection_task)) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) override;

    static std::string getName() {
        return "AnomalyDetection";
    }

  private:
    void logParamsStats(const std::string &pred_label, const double &pred_score, const double &image_threshold_norm);
};

} // namespace post_processing
