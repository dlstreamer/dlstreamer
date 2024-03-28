/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_roi_converter.h"
#include <opencv2/opencv.hpp>
#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {
    
struct InferDimsCHW
{
    unsigned int c, h, w;
    void set(unsigned int c, unsigned int h, unsigned w)
    {
        this->c = c;
        this->h = h;
        this->w = w;
    }
};

class Resnet10Converter : public BlobToROIConverter {
    
  protected:

    // FIXME: move roi_scale to coordinates restorer or attacher
    void parseOutputBlob(const InferDimsCHW& covLayerDims, const InferDimsCHW& bboxLayerDims, const float *outputCovBuf, const float *outputBboxBuf,
                                      int numClassesToParse, std::vector<DetectedObject> &objects) const;

  public:
    Resnet10Converter(BlobToMetaConverter::Initializer initializer, double confidence_threshold)
        : BlobToROIConverter(std::move(initializer), confidence_threshold, true, 0.4) {
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "resnet_10";
    }

    static std::string getDepricatedName() {
        return "tensor_to_bbox_resnet_10";
    }
};
} // namespace post_processing
