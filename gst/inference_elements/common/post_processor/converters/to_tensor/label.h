/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_tensor_converter.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post_processing {

class LabelConverter : public BlobToTensorConverter {
  public:
    enum class Method { Max, SoftMax, Compound, Index, Default = Max };

    LabelConverter(BlobToMetaConverter::Initializer initializer);

    TensorsTable convert(const OutputBlobs &output_blobs) const override;

    static std::string getName() {
        return "label";
    }
    static std::string getDepricatedName() {
        return "tensor_to_label";
    }

  private:
    Method _method = Method::Max;
};
} // namespace post_processing
