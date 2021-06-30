/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "post_proc_common.h"

#include "inference_backend/input_image_layer_descriptor.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <map>
#include <memory>
#include <string>

struct InferenceFrame;

namespace post_processing {
class CoordinatesRestorer {
  protected:
    const ModelImageInputInfo input_info;

  public:
    CoordinatesRestorer(const ModelImageInputInfo &input_info) : input_info(input_info) {
    }

    virtual void restore(TensorsTable &tensors_batch, const InferenceFrames &frames) = 0;

    using Ptr = std::unique_ptr<CoordinatesRestorer>;

    virtual ~CoordinatesRestorer() = default;
};

class ROICoordinatesRestorer : public CoordinatesRestorer {
  protected:
    void clipNormalizedRect(double &real_x_min, double &real_y_min, double &real_x_max, double &real_y_max);
    void getActualCoordinates(int orig_image_width, int orig_image_height,
                              const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info, double &real_x_min,
                              double &real_y_min, double &real_x_max, double &real_y_max);
    GstVideoRegionOfInterestMeta *findDetectionMeta(const InferenceFrame &frame);
    void updateCoordinatesToFullFrame(double &x_min, double &y_min, double &x_max, double &y_max,
                                      const InferenceFrame &frame);
    void getAbsoluteCoordinates(int orig_image_width, int orig_image_height, double real_x_min, double real_y_min,
                                double real_x_max, double real_y_max, uint32_t &abs_x, uint32_t &abs_y, uint32_t &abs_w,
                                uint32_t &abs_h);
    void getRealCoordinates(GstStructure *detection_tensor, double &x_min_real, double &y_min_real, double &x_max_real,
                            double &y_max_real);
    void getCoordinates(GstStructure *detection_tensor, const InferenceFrame &frame, double &x_min_real,
                        double &y_min_real, double &x_max_real, double &y_max_real, uint32_t &x_abs, uint32_t &y_abs,
                        uint32_t &w_abs, uint32_t &h_abs);

  public:
    ROICoordinatesRestorer(const ModelImageInputInfo &input_info) : CoordinatesRestorer(input_info) {
    }

    virtual void restore(TensorsTable &tensors_batch, const InferenceFrames &frames) override;
};

class KeypointsCoordinatesRestorer : public CoordinatesRestorer {
  protected:
    std::vector<float> extractKeypointsData(GstStructure *s);

    void restoreActualCoordinates(const InferenceFrame &frame, float &real_x, float &real_y);
    GstVideoRegionOfInterestMeta *findDetectionMeta(const InferenceFrame &frame);
    void updateCoordinatesToFullFrame(float &x, float &y, const InferenceFrame &frame);

  public:
    KeypointsCoordinatesRestorer(const ModelImageInputInfo &input_info) : CoordinatesRestorer(input_info) {
    }

    virtual void restore(TensorsTable &tensors_batch, const InferenceFrames &frames) override;
};

} // namespace post_processing
