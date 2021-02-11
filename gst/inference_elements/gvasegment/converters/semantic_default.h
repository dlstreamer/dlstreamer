/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "converter.h"

namespace SegmentationPlugin {
namespace Converters {

class SemanticDefaultConverter : public Converter {
  public:
    SemanticDefaultConverter(int show_zero_class);
    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, const std::string &model_name,
                 const std::string &layer_name, GValueArray *labels_raw, GstStructure *segmentation_result) override;
};
} // namespace Converters
} // namespace SegmentationPlugin
