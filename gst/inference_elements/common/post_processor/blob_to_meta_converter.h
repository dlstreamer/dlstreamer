/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "environment_variable_options_reader.h"
#include "feature_toggling/ifeature_toggle.h"
#include "gst_smart_pointer_types.hpp"
#include "inference_backend/image_inference.h"
#include "post_proc_common.h"
#include "runtime_feature_toggler.h"
#include "tensor.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>

namespace post_processing {

class BlobToMetaConverter {
  private:
    std::string model_name;
    ModelImageInputInfo input_image_info;

    GstStructureUniquePtr model_proc_output_info;
    std::vector<std::string> labels;

  protected:
    const ModelImageInputInfo &getModelInputImageInfo() const {
        return input_image_info;
    }
    const std::string &getModelName() const {
        return model_name;
    }

    const std::vector<std::string> &getLabels() const {
        return labels;
    }
    const std::string &getLabelByLabelId(size_t label_id) const {
        static const std::string empty_label = "";
        return (getLabels().empty()) ? empty_label : getLabels().at(label_id);
    }
    const GstStructureUniquePtr &getModelProcOutputInfo() const {
        return model_proc_output_info;
    }

  public:
    BlobToMetaConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                        GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels);

    virtual TensorsTable convert(const OutputBlobs &output_blobs) const = 0;

    using Ptr = std::unique_ptr<BlobToMetaConverter>;
    static Ptr create(GstStructure *model_proc_output_info, int inference_type,
                      const ModelImageInputInfo &input_image_info, const std::string &model_name,
                      const std::vector<std::string> &labels, const std::string &displayed_layer_name_in_meta);

    virtual ~BlobToMetaConverter() = default;
};

class BlobToTensorConverter : public BlobToMetaConverter {
  protected:
    std::unique_ptr<FeatureToggling::Runtime::RuntimeFeatureToggler> raw_tesor_copying;
    GVA::Tensor createTensor() const;

    struct RawTensorCopyingToggle final : FeatureToggling::Base::IFeatureToggle<RawTensorCopyingToggle> {
        static const std::string id;
        static const std::string deprecation_message;
    };

  public:
    BlobToTensorConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                          GstStructureUniquePtr model_proc_output_info, const std::vector<std::string> &labels)
        : BlobToMetaConverter(model_name, input_image_info, std::move(model_proc_output_info), labels),
          raw_tesor_copying(new FeatureToggling::Runtime::RuntimeFeatureToggler()) {
        FeatureToggling::Runtime::EnvironmentVariableOptionsReader env_var_options_reader;
        raw_tesor_copying->configure(env_var_options_reader.read("ENABLE_GVA_FEATURES"));
    }

    TensorsTable convert(const OutputBlobs &output_blobs) const = 0;
};

} // namespace post_processing
