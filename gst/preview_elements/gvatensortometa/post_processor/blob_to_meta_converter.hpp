/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gst_smart_pointer_types.hpp"
#include "inference_backend/image_inference.h"
#include "tensor.h"

#include <gst/gst.h>

#include "post_proc_common.hpp"
#include <map>
#include <memory>
#include <string>

namespace PostProcessing {

class BlobToMetaConverter {
  private:
    // FIXME: remove static
    std::string model_name;
    ModelImageInputInfo input_image_info;

    GstStructureUniquePtr model_proc_output_info;
    std::string converter_name;
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
    std::string getLabelByLabelId(size_t label_id) const {
        return (getLabels().empty()) ? "" : getLabels().at(label_id);
    }
    const std::string &getConverterName() const {
        return converter_name;
    }
    const GstStructureUniquePtr &getModelProcOutputInfo() const {
        return model_proc_output_info;
    }

  public:
    BlobToMetaConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                        GstStructure *model_proc_output_info, const std::vector<std::string> &labels);

    virtual MetasTable convert(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) const = 0;

    using Ptr = std::unique_ptr<BlobToMetaConverter>;
    static Ptr create(GstStructure *model_proc_output_info, const ModelImageInputInfo &input_image_info,
                      const std::string &model_name, const std::vector<std::string> &labels);

    virtual ~BlobToMetaConverter() = default;
};

class BlobToTensorConverter : public BlobToMetaConverter {
  protected:
    GVA::Tensor createTensor() const;

  public:
    BlobToTensorConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                          GstStructure *_model_proc_output_info, const std::vector<std::string> &labels)
        : BlobToMetaConverter(model_name, input_image_info, _model_proc_output_info, labels) {
    }

    MetasTable convert(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) const = 0;
};

} // namespace PostProcessing
