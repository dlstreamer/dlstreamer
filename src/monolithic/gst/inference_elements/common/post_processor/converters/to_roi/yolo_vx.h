/*******************************************************************************
 * Copyright (C) 2022 Videonetics Technology Pvt Ltd
 * Copyright (C) 2024-2025 Videonetics Technology Pvt Ltd
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"
#include <iostream>
#include <utility>
#include <vector>

namespace post_processing {

struct GridAndStride {
    int grid0;
    int grid1;
    int stride;
};

class YOLOvxConverter : public BlobToROIConverter {
  private:
    const size_t classes_number;
    const std::vector<int> strides;
    const std::vector<GridAndStride> grid_strides;

    void generateYoloxProposals(const float *blob_data, const std::vector<size_t> &blob_dims, size_t blob_size,
                                std::vector<DetectedObject> &objects) const;
    void parseOutputBlob(const float *blob_data, const std::vector<size_t> &blob_dims, size_t blob_size,
                         std::vector<DetectedObject> &objects) const;

  public:
    static BlobToMetaConverter::Ptr create(BlobToMetaConverter::Initializer initializer,
                                           const std::string &converter_name, double confidence_threshold);
    YOLOvxConverter(BlobToMetaConverter::Initializer initializer, double confidence_threshold, bool need_nms,
                    double iou_threshold, const size_t classes_number, const std::vector<int> &strides,
                    const std::vector<GridAndStride> &grid_strides);
    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "yolo_vx";
    }
};
} // namespace post_processing
