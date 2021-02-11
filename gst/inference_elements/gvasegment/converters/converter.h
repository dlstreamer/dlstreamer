/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include <processor_types.h>

namespace SegmentationPlugin {
namespace Converters {

class Converter {
  protected:
    int show_zero_class;

  public:
    virtual ~Converter() = default;
    virtual bool process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                         const std::vector<std::shared_ptr<InferenceFrame>> &frames, const std::string &model_name,
                         const std::string &layer_name, GValueArray *labels_raw, GstStructure *segmentation_result) = 0;
    static std::string getConverterType(const GstStructure *s = nullptr);
    static Converter *create(const GstStructure *model_proc_info,
                             const std::vector<ModelInputProcessorInfo::Ptr> *inputLayers);

  protected:
    void getLabelByLabelId(GValueArray *labels, int label_id, std::string &out_label);
    std::vector<uint32_t> probabilitiesToIndex(const float *data, size_t batches, size_t channels, size_t height,
                                               size_t width);
    void copySemanticInfoToGstStructure(const float *data, std::vector<size_t> dims, const std::string &model_name,
                                        const std::string &layer_name,
                                        InferenceBackend::OutputBlob::Precision precision,
                                        InferenceBackend::OutputBlob::Layout layout, size_t batch_size,
                                        size_t batch_index, GstStructure *tensor_structure);
};

} // namespace Converters
} // namespace SegmentationPlugin
