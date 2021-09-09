/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "blob_to_meta_converter.h"
#include "coordinates_restorer.h"
#include "meta_attacher.h"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace post_processing {

class ConverterFacade {
  private:
    void setLayerNames(const GstStructure *s);
    OutputBlobs extractProcessedOutputBlobs(const OutputBlobs &all_output_blobs) const;

    CoordinatesRestorer::Ptr createCoordinatesRestorer(int inference_type, const ModelImageInputInfo &input_image_info,
                                                       GstStructure *model_proc_output_info = nullptr);

  protected:
    std::unordered_set<std::string> layer_names_to_process;
    BlobToMetaConverter::Ptr blob_to_meta;
    CoordinatesRestorer::Ptr coordinates_restorer;
    MetaAttacher::Ptr meta_attacher;

  public:
    ConverterFacade(std::unordered_set<std::string> all_layer_names, GstStructure *model_proc_output_info,
                    int inference_type, int inference_region, const ModelImageInputInfo &input_image_info,
                    const std::string &model_name, const std::vector<std::string> &labels);
    ConverterFacade(GstStructure *model_proc_output_info, int inference_type, int inference_region,
                    const ModelImageInputInfo &input_image_info, const std::string &model_name,
                    const std::vector<std::string> &labels);

    void convert(const OutputBlobs &all_output_blobs, InferenceFrames &frames) const;

    ConverterFacade() = default;
    ConverterFacade(const ConverterFacade &) = default;
    ConverterFacade(ConverterFacade &&) = default;
    ConverterFacade &operator=(const ConverterFacade &) = default;
    ConverterFacade &operator=(ConverterFacade &&) = default;

    ~ConverterFacade() = default;
};

} // namespace post_processing
