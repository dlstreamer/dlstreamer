/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "converter.h"

namespace SegmentationPlugin {
namespace Converters {

class PixelLinkConverter : public Converter {
  private:
    double cls_conf_threshold = 0.5;
    double link_conf_threshold = 0.5;

  protected:
    struct OutputLayersName {
      private:
        bool are_valid_layers_names = false;

      public:
        const std::string link_logits = "model/link_logits_/add";
        const std::string segm_logits = "model/segm_logits/add";

        bool checkBlobCorrectness(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs);
    };
    OutputLayersName layers_name;

  public:
    PixelLinkConverter(double cls_conf_threshold, double link_conf_threshold, int show_zero_class);
    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, const std::string &model_name,
                 const std::string &layer_name, GValueArray *labels_raw, GstStructure *segmentation_result) override;
};
} // namespace Converters
} // namespace SegmentationPlugin
