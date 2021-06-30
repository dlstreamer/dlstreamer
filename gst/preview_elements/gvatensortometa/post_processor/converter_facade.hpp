/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <capabilities/types.hpp>

#include "blob_to_meta_converter.hpp"
#include "meta_attacher.hpp"

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class ConverterFacade {
  private:
    void setLayerNames(const GstStructure *s);
    std::map<std::string, InferenceBackend::OutputBlob::Ptr>
    extractProcessedOutputBlobs(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &all_output_blobs) const;

  protected:
    std::unordered_set<std::string> processed_layer_names;
    PostProcessing::BlobToMetaConverter::Ptr blob_to_meta;
    PostProcessing::MetaAttacher::Ptr meta_attacher;

  public:
    ConverterFacade(std::unordered_set<std::string> all_layer_names, const ModelImageInputInfo &input_image_info,
                    const std::string &model_name);
    ConverterFacade(std::unordered_set<std::string> all_layer_names, GstStructure *model_proc_output_info,
                    const ModelImageInputInfo &input_image_info, const std::string &model_name,
                    const std::vector<std::string> &labels);
    ConverterFacade(GstStructure *model_proc_output_info, const ModelImageInputInfo &input_image_info,
                    const std::string &model_name, const std::vector<std::string> &labels);

    void convert(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &all_output_blobs,
                 GstBuffer *buffer) const;

    ConverterFacade() = default;
    ConverterFacade(const ConverterFacade &) = default;
    ConverterFacade(ConverterFacade &&) = default;
    ConverterFacade &operator=(const ConverterFacade &) = default;
    ConverterFacade &operator=(ConverterFacade &&) = default;

    ~ConverterFacade() = default;
};
