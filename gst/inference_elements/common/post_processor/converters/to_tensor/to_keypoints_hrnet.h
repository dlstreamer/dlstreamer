/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "to_keypoints.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class ToKeypointsHRnetConverter : public ToKeypointsConverter {
  public:
    ToKeypointsHRnetConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                              GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels)
        : ToKeypointsConverter(model_name, input_image_info, std::move(model_proc_output_info), labels) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "tensor_to_keypoints_hrnet";
    }
};
} // namespace post_processing
