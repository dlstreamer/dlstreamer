/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class OVDefaultConverter : public BlobToROIConverter {
  protected:
    // FIXME: move roi_scale to coordinates restorer or attacher
    void parseOutputBlob(const InferenceBackend::OutputBlob::Ptr &blob, DetectedObjectsTable &objects,
                         double roi_scale) const;

  public:
    OVDefaultConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                       GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels,
                       double confidence_threshold)
        : BlobToROIConverter(model_name, input_image_info, std::move(model_proc_output_info), labels,
                             confidence_threshold, false, 0.0) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;
};
} // namespace post_processing
