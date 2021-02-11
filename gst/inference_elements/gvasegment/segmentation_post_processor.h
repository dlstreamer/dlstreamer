/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "processor_types.h"

#include "gst_smart_pointer_types.hpp"

class InferenceImpl;

namespace SegmentationPlugin {

namespace Converters {
class Converter;
}

using ConverterUniquePtr = std::unique_ptr<SegmentationPlugin::Converters::Converter>;

struct LayerInfo {
    LayerInfo();
    LayerInfo(ConverterUniquePtr converter, const GValueArray *labels, const GstStructure *model_proc_info);
    LayerInfo(ConverterUniquePtr converter, GValueArrayUniquePtr labels, GstStructureUniquePtr model_proc_info);
    ConverterUniquePtr converter;
    GValueArrayUniquePtr labels;
    GstStructureUniquePtr model_proc_info;
};

using LayersInfoMap = std::map<std::string, LayerInfo>;

class SegmentationPostProcessor : public PostProcessor {
  private:
    LayersInfoMap layers_info;
    std::string model_name;

  public:
    SegmentationPostProcessor(const InferenceImpl *inference_impl);
    PostProcessor::ExitStatus process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                      std::vector<std::shared_ptr<InferenceFrame>> &frames);
    ~SegmentationPostProcessor() = default;
};

} // namespace SegmentationPlugin
