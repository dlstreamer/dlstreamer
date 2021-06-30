/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "post_processor/blob_to_meta_converter.hpp"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PostProcessing {

class ToLabelConverter : public BlobToTensorConverter {
  private:
    void find_max_element_index(const float *array, int len, int &index, float &value) const;

  public:
    ToLabelConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                     GstStructure *model_proc_output_info, const std::vector<std::string> &labels)
        : BlobToTensorConverter(model_name, input_image_info, model_proc_output_info, labels) {
    }

    MetasTable convert(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) const override;
};
} // namespace PostProcessing
