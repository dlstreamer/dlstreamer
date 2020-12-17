/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "processor_types.h"

#include "gst_smart_pointer_types.hpp"

namespace GVA {
class Tensor;
}

class InferenceImpl;

namespace ClassificationPlugin {

using ConverterFunctionType = std::function<void(GVA::Tensor &, GValueArray *)>;

struct ClassificationLayerInfo {
    ConverterFunctionType converter;
    GValueArrayUniquePtr labels;
    GstStructureUniquePtr model_proc_info;

    ClassificationLayerInfo();
    ClassificationLayerInfo(const std::string &layer_name);
    ClassificationLayerInfo(ConverterFunctionType converter, const GValueArray *labels,
                            const GstStructure *model_proc_info);
    ClassificationLayerInfo(ConverterFunctionType converter, GValueArrayUniquePtr labels,
                            GstStructureUniquePtr model_proc_info);
};

using ClassificationLayersInfoMap = std::map<std::string, ClassificationLayerInfo>;

class ClassificationPostProcessor : public PostProcessor {
  private:
    std::string model_name;
    ClassificationLayersInfoMap layers_info;

    void fillLayersInfoIfEmpty(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs);
    PostProcessor::ExitStatus pushClassificationResultToFrames(InferenceBackend::OutputBlob::Ptr &blob,
                                                               unsigned int blob_id,
                                                               ClassificationLayerInfo &layer_info,
                                                               const std::string &layer_name,
                                                               std::vector<std::shared_ptr<InferenceFrame>> &frames);

  public:
    ClassificationPostProcessor(const InferenceImpl *inference_impl);
    ExitStatus process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                       std::vector<std::shared_ptr<InferenceFrame>> &frames) override;
};

} // namespace ClassificationPlugin
