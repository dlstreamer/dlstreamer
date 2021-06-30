/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converter_facade.hpp"

#include "blob_to_meta_converter.hpp"
#include "meta_attacher.hpp"

#include "gva_base_inference.h"

#include <gst/gst.h>

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace PostProcessing;

void ConverterFacade::setLayerNames(const GstStructure *s) {
    if (not gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names"))
        throw std::runtime_error("model proc does not have \"layer_name\" information.");
    if (gst_structure_has_field(s, "layer_name") and gst_structure_has_field(s, "layer_names"))
        throw std::runtime_error("model proc has \"layer_name\" and \"layer_names\" information.");

    if (gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names")) {
        processed_layer_names.emplace(gst_structure_get_string(s, "layer_name"));
    }

    if (gst_structure_has_field(s, "layer_names") and not gst_structure_has_field(s, "layer_name")) {
        GValueArray *arr = NULL;
        gst_structure_get_array(const_cast<GstStructure *>(s), "layer_names", &arr);
        if (arr and arr->n_values) {
            processed_layer_names.reserve(arr->n_values);
            for (guint i = 0; i < arr->n_values; ++i)
                processed_layer_names.emplace(g_value_get_string(g_value_array_get_nth(arr, i)));
            g_value_array_free(arr);
        } else {
            throw std::runtime_error("\"layer_names\" array is null.");
        }
    }

    return;
}

ConverterFacade::ConverterFacade(std::unordered_set<std::string> all_layer_names,
                                 const ModelImageInputInfo &input_image_info, const std::string &model_name)
    : processed_layer_names(all_layer_names) {
    meta_attacher = MetaAttacher::create(input_image_info);

    GstStructureUniquePtr model_proc_output_info =
        GstStructureUniquePtr(gst_structure_new_empty("ANY"), gst_structure_free);
    std::vector<std::string> labels{};

    blob_to_meta = BlobToMetaConverter::create(model_proc_output_info.get(), input_image_info, model_name, labels);
}

ConverterFacade::ConverterFacade(std::unordered_set<std::string> all_layer_names, GstStructure *model_proc_output_info,
                                 const ModelImageInputInfo &input_image_info, const std::string &model_name,
                                 const std::vector<std::string> &labels)
    : processed_layer_names(all_layer_names) {
    meta_attacher = MetaAttacher::create(input_image_info);
    blob_to_meta = BlobToMetaConverter::create(model_proc_output_info, input_image_info, model_name, labels);
}

ConverterFacade::ConverterFacade(GstStructure *model_proc_output_info, const ModelImageInputInfo &input_image_info,
                                 const std::string &model_name, const std::vector<std::string> &labels) {
    if (model_proc_output_info == nullptr) {
        throw std::runtime_error("Can not get model_proc output information.");
    }

    setLayerNames(model_proc_output_info);
    meta_attacher = MetaAttacher::create(input_image_info);
    blob_to_meta = BlobToMetaConverter::create(model_proc_output_info, input_image_info, model_name, labels);
}

std::map<std::string, InferenceBackend::OutputBlob::Ptr> ConverterFacade::extractProcessedOutputBlobs(
    const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &all_output_blobs) const {
    if (all_output_blobs.empty())
        throw std::invalid_argument("Output_blobs is empty.");

    // FIXME: c++17 has extract method for it
    auto processed_output_blobs = all_output_blobs;
    for (auto it = processed_output_blobs.begin(); it != processed_output_blobs.end();) {
        if (processed_layer_names.find(it->first) == processed_layer_names.cend())
            it = processed_output_blobs.erase(it);
        else
            ++it;
    }
    return processed_output_blobs;
}

void ConverterFacade::convert(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &all_output_blobs,
                              GstBuffer *buffer) const {
    const auto processed_output_blobs = extractProcessedOutputBlobs(all_output_blobs);
    auto metas = blob_to_meta->convert(processed_output_blobs);
    meta_attacher->attach(metas, buffer);
}
