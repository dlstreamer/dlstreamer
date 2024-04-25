/*******************************************************************************
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"
#include "inference_backend/image_inference.h"
#include <opencv2/opencv.hpp>

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class YOLOv8Converter : public BlobToROIConverter {
  protected:
    // FIXME: move roi_scale to coordinates restorer or attacher
    void parseOutputBlob(const float *data, const std::vector<size_t> &dims,
                         std::vector<DetectedObject> &objects) const;

  public:
    YOLOv8Converter(BlobToMetaConverter::Initializer initializer, double confidence_threshold)
        : BlobToROIConverter(std::move(initializer), confidence_threshold, true, 0.4) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "yolo_v8";
    }

    static std::string getDepricatedName() {
        return "tensor_to_bbox_yolo_v8";
    }
};
} // namespace post_processing