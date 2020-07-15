/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "converters/yolo_base.h"

namespace DetectionPlugin {
namespace Converters {

class YOLOV3Converter : public YOLOConverter {
  protected:
    const size_t classes_number;
    const std::map<size_t, std::vector<size_t>> masks;
    const size_t coords;
    const size_t num;
    const float input_size;

    uint32_t entryIndex(uint32_t side, uint32_t lcoords, uint32_t lclasses, uint32_t location, uint32_t entry);

  public:
    YOLOV3Converter(size_t classes_number, std::vector<float> anchors, std::map<size_t, std::vector<size_t>> masks,
                    double iou_threshold = 0.5, size_t num = 3, size_t coords = 4, float input_size = 416);

    bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                 const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                 double confidence_threshold, GValueArray *labels);
};

} // namespace Converters
} // namespace DetectionPlugin
