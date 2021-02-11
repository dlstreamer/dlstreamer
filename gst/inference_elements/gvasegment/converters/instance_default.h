/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "converter.h"

namespace SegmentationPlugin {
namespace Converters {

class InstanceDefaultConverter : public Converter {
  private:
    size_t net_width;
    size_t net_height;
    double threshold;

  protected:
    struct OutputLayersName {
      private:
        bool are_valid_layers_names = false;

      public:
        const std::string boxes = "boxes";
        const std::string classes = "classes";
        const std::string raw_masks = "raw_masks";
        const std::string scores = "scores";

        bool checkBlobCorrectness(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs);
    };
    OutputLayersName layers_name;

  public:
    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, const std::string &model_name,
                 const std::string &layer_name, GValueArray *labels_raw, GstStructure *segmentation_result) override;
    InstanceDefaultConverter(size_t height, size_t width, double threshold);
};
} // namespace Converters
} // namespace SegmentationPlugin
