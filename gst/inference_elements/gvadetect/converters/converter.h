/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "detection_post_processor.h"
#include <processor_types.h>

namespace DetectionPlugin {
namespace Converters {

class Converter {
  public:
    virtual ~Converter() = default;
    virtual bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                         double confidence_threshold, GValueArray *labels) = 0;
    void addRoi(GstBuffer *buffer, GstVideoInfo *info,
                const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info, double x, double y, double w,
                double h, int label_id, double confidence, GstStructure *detection_tensor, GValueArray *labels);
    void clipNormalizedRect(double &x, double &y, double &w, double &h);
    void getLabelByLabelId(GValueArray *labels, int label_id, gchar **out_label);

    static std::string getConverterType(const GstStructure *s = nullptr);
    static Converter *create(const GstStructure *output_model_proc_info, const ModelInputInfo &input_info);

  protected:
    static ModelInputInfo input_info;
    void getActualCoordinates(int orig_image_width, int orig_image_height,
                              const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info, double &real_x,
                              double &real_y, double &real_w, double &real_h, uint32_t &abs_x, uint32_t &abs_y,
                              uint32_t &abs_w, uint32_t &abs_h);
};

} // namespace Converters
} // namespace DetectionPlugin
