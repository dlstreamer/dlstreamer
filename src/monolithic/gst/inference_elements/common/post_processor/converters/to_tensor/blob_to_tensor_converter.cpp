/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blob_to_tensor_converter.h"

#include <exception>

using namespace post_processing;

BlobToTensorConverter::BlobToTensorConverter(BlobToMetaConverter::Initializer initializer)
    : BlobToMetaConverter(std::move(initializer)),
      raw_tensor_copying(new FeatureToggling::Runtime::RuntimeFeatureToggler()) {
    FeatureToggling::Runtime::EnvironmentVariableOptionsReader env_var_options_reader;
    raw_tensor_copying->configure(env_var_options_reader.read("ENABLE_GVA_FEATURES"));
}

GVA::Tensor BlobToTensorConverter::createTensor() const {
    GstStructure *tensor_data = nullptr;
    if (getModelProcOutputInfo()) {
        tensor_data = copy(getModelProcOutputInfo().get(), gst_structure_copy);
    } else {
        throw std::runtime_error("Failed to initialize classification result structure: model-proc is null.");
    }
    if (!tensor_data) {
        throw std::runtime_error("Failed to initialize classification result tensor.");
    }

    return GVA::Tensor(tensor_data);
}

const std::string BlobToTensorConverter::RawTensorCopyingToggle::id = "disable-tensor-copying";
const std::string BlobToTensorConverter::RawTensorCopyingToggle::deprecation_message =
    "In pipelines with gvaclassify, in addition to classification results, a raw inference tensor is added to the "
    "metadata. This functionality will be removed in future releases. Set environment variable "
    "ENABLE_GVA_FEATURES=disable-tensor-copying to disable copying to "
    "frame metadata of raw tensor after inference.";
