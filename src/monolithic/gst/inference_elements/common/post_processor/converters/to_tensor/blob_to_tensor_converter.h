/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
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

const std::string DEFAULT_ANOMALY_DETECTION_TASK = "classification";

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

    virtual TensorsTable convert(const OutputBlobs &output_blobs) = 0;

    static BlobToMetaConverter::Ptr create(BlobToMetaConverter::Initializer initializer,
                                           const std::string &converter_name, const std::string &custom_postproc_lib);
};

} // namespace post_processing
