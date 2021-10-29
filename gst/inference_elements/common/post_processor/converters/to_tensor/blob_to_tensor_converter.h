/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include "feature_toggling/ifeature_toggle.h"
#include "post_processor/blob_to_meta_converter.h"
#include "post_processor/post_proc_common.h"
#include "runtime_feature_toggler.h"

#include <memory>
#include <string>

namespace post_processing {

class BlobToTensorConverter : public BlobToMetaConverter {
  protected:
    std::unique_ptr<FeatureToggling::Runtime::RuntimeFeatureToggler> raw_tensor_copying;
    GVA::Tensor createTensor() const;

    struct RawTensorCopyingToggle final : FeatureToggling::Base::IFeatureToggle<RawTensorCopyingToggle> {
        static const std::string id;
        static const std::string deprecation_message;
    };

  public:
    BlobToTensorConverter(BlobToMetaConverter::Initializer initializer);

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;
};

} // namespace post_processing
