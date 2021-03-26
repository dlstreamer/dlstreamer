/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once
#include "detection_post_processor.h"
#include "gstgvadetect.h"
#include "gva_utils.h"
#include <processor_types.h>
namespace DetectionPlugin {
namespace Converters {

class Converter {
  public:
    virtual ~Converter() = default;
    virtual bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                         double confidence_threshold, GValueArray *labels) = 0;

    static std::string getConverterType(const GstStructure *s = nullptr);
    static Converter *create(const GstStructure *output_model_proc_info, const ModelInputInfo &input_info);

  protected:
    static ModelInputInfo input_info;
    void getActualCoordinates(int orig_image_width, int orig_image_height,
                              const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info, float &real_x,
                              float &real_y, float &real_w, float &real_h, uint32_t &abs_x, uint32_t &abs_y,
                              uint32_t &abs_w, uint32_t &abs_h);
    void addRoi(const std::shared_ptr<InferenceFrame> frame, float x, float y, float w, float h, int label_id,
                double confidence, GstStructure *detection_tensor, GValueArray *labels);
    void clipNormalizedRect(float &x, float &y, float &w, float &h);
    void getLabelByLabelId(GValueArray *labels, int label_id, gchar **out_label);
};

} // namespace Converters
} // namespace DetectionPlugin
