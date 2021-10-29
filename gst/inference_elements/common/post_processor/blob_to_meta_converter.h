/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "environment_variable_options_reader.h"
#include "gst_smart_pointer_types.hpp"
#include "inference_backend/image_inference.h"
#include "post_proc_common.h"
#include "tensor.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>

namespace post_processing {

class BlobToMetaConverter {
  public:
    struct Initializer {
        std::string model_name;
        ModelImageInputInfo input_image_info;
        ModelOutputsInfo outputs_info;

        GstStructureUniquePtr model_proc_output_info;
        std::vector<std::string> labels;
    };

  private:
    const std::string model_name;
    const ModelImageInputInfo input_image_info;
    const ModelOutputsInfo outputs_info;

    GstStructureUniquePtr model_proc_output_info;
    const std::vector<std::string> labels;

  protected:
    const ModelImageInputInfo &getModelInputImageInfo() const {
        return input_image_info;
    }
    const ModelOutputsInfo &getModelOutputsInfo() const {
        return outputs_info;
    }
    const std::string &getModelName() const {
        return model_name;
    }

    const GstStructureUniquePtr &getModelProcOutputInfo() const {
        return model_proc_output_info;
    }
    const std::vector<std::string> &getLabels() const {
        return labels;
    }
    const std::string &getLabelByLabelId(size_t label_id) const {
        static const std::string empty_label = "";
        return (getLabels().empty()) ? empty_label : getLabels().at(label_id);
    }

  public:
    BlobToMetaConverter(Initializer initializer);

    virtual TensorsTable convert(const OutputBlobs &output_blobs) const = 0;

    using Ptr = std::unique_ptr<BlobToMetaConverter>;
    static Ptr create(Initializer initializer, ConverterType converter_type,
                      const std::string &displayed_layer_name_in_meta);

    virtual ~BlobToMetaConverter() = default;
};

} // namespace post_processing
