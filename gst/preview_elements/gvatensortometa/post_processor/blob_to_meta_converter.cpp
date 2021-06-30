/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blob_to_meta_converter.hpp"

#include "converters/to_tensor/to_label.hpp"
#include "copy_blob_to_gststruct.h"

#include "gva_base_inference.h"

#include <gst/gst.h>

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace PostProcessing;

namespace {
std::string getConverterType(GstStructure *s) {
    if (s == nullptr || !gst_structure_has_field(s, "converter"))
        return "";

    std::string converter_type = gst_structure_get_string(s, "converter");
    if (converter_type.empty())
        throw std::runtime_error("model_proc's output_processor has empty converter.");
    return converter_type;
}
} // namespace

BlobToMetaConverter::BlobToMetaConverter(const std::string &model_name, const ModelImageInputInfo &input_image_info,
                                         GstStructure *_model_proc_output_info, const std::vector<std::string> &labels)
    : model_name(model_name), input_image_info(input_image_info), labels(labels) {
    if (_model_proc_output_info == nullptr)
        model_proc_output_info = GstStructureUniquePtr(gst_structure_new_empty("ANY"), gst_structure_free);
    else
        model_proc_output_info = GstStructureUniquePtr(gst_structure_copy(_model_proc_output_info), gst_structure_free);

    converter_name = getConverterType(_model_proc_output_info);
}

BlobToMetaConverter::Ptr BlobToMetaConverter::create(GstStructure *model_proc_output_info,
                                                     const ModelImageInputInfo &input_image_info,
                                                     const std::string &model_name,
                                                     const std::vector<std::string> &labels) {
    std::string converter_name = getConverterType(model_proc_output_info);

    if (converter_name == "tensor_to_label") {
        return BlobToMetaConverter::Ptr(
            new ToLabelConverter(model_name, input_image_info, model_proc_output_info, labels));
    } else {
        return nullptr;
    }
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
