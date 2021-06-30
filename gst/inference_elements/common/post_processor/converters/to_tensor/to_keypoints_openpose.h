/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "to_keypoints.h"

#include "human_pose_extractor/human_pose_extractor.h"
#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class ToKeypointsOpenPoseConverter : public ToKeypointsConverter {
  private:
    const HumanPoseExtractor extractor;

  public:
    ToKeypointsOpenPoseConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                                 GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                                 size_t keypoints_number)
        : ToKeypointsConverter(model_name, input_image_info, std::move(model_proc_output_info), labels),
          extractor(keypoints_number) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;
};
} // namespace post_processing
