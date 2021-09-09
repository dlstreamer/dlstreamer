/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converter_facade.h"

#include "blob_to_meta_converter.h"
#include "meta_attacher.h"

#include "gva_base_inference.h"

#include <gst/gst.h>

#include <exception>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace post_processing;

namespace {
std::string getDisplayedLayerNameInMeta(std::vector<std::string> &&layer_names) {
    if (layer_names.empty())
        throw std::invalid_argument("Layer names is empty.");

    std::string displayed_layer_name = layer_names.back();
    layer_names.pop_back();
    for (const auto &layer_name : layer_names)
        displayed_layer_name = layer_name + "\\" + displayed_layer_name;

    return displayed_layer_name;
}
} // namespace

void ConverterFacade::setLayerNames(const GstStructure *s) {
    if (not gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names"))
        throw std::runtime_error("model proc does not have \"layer_name\" information.");
    if (gst_structure_has_field(s, "layer_name") and gst_structure_has_field(s, "layer_names"))
        throw std::runtime_error("model proc has \"layer_name\" and \"layer_names\" information.");

    if (gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names")) {
        layer_names_to_process.emplace(gst_structure_get_string(s, "layer_name"));
    }

    if (gst_structure_has_field(s, "layer_names") and not gst_structure_has_field(s, "layer_name")) {
        GValueArray *arr = nullptr;
        gst_structure_get_array(const_cast<GstStructure *>(s), "layer_names", &arr);
        if (arr and arr->n_values) {
            layer_names_to_process.reserve(arr->n_values);
            for (guint i = 0; i < arr->n_values; ++i)
                layer_names_to_process.emplace(g_value_get_string(g_value_array_get_nth(arr, i)));
            g_value_array_free(arr);
        } else {
            throw std::runtime_error("\"layer_names\" array is null.");
        }
    }
}

CoordinatesRestorer::Ptr ConverterFacade::createCoordinatesRestorer(int inference_type,
                                                                    const ModelImageInputInfo &input_image_info,
                                                                    GstStructure *model_proc_output_info) {
    if (inference_type == GST_GVA_DETECT_TYPE)
        return CoordinatesRestorer::Ptr(new ROICoordinatesRestorer(input_image_info));

    if (model_proc_output_info != nullptr)
        if (gst_structure_has_field(model_proc_output_info, "point_names"))
            return CoordinatesRestorer::Ptr(new KeypointsCoordinatesRestorer(input_image_info));

    return nullptr;
}

ConverterFacade::ConverterFacade(std::unordered_set<std::string> all_layer_names, GstStructure *model_proc_output_info,
                                 int inference_type, int inference_region, const ModelImageInputInfo &input_image_info,
                                 const std::string &model_name, const std::vector<std::string> &labels)
    : layer_names_to_process(all_layer_names) {
    blob_to_meta =
        BlobToMetaConverter::create(model_proc_output_info, inference_type, input_image_info, model_name, labels,
                                    getDisplayedLayerNameInMeta(std::vector<std::string>(
                                        layer_names_to_process.cbegin(), layer_names_to_process.cend())));
    coordinates_restorer = createCoordinatesRestorer(inference_type, input_image_info, model_proc_output_info);
    meta_attacher = MetaAttacher::create(inference_type, inference_region, input_image_info);
}

ConverterFacade::ConverterFacade(GstStructure *model_proc_output_info, int inference_type, int inference_region,
                                 const ModelImageInputInfo &input_image_info, const std::string &model_name,
                                 const std::vector<std::string> &labels) {
    if (model_proc_output_info == nullptr) {
        throw std::runtime_error("Can not get model_proc output information.");
    }

    setLayerNames(model_proc_output_info);

    blob_to_meta =
        BlobToMetaConverter::create(model_proc_output_info, inference_type, input_image_info, model_name, labels,
                                    getDisplayedLayerNameInMeta(std::vector<std::string>(
                                        layer_names_to_process.cbegin(), layer_names_to_process.cend())));
    coordinates_restorer = createCoordinatesRestorer(inference_type, input_image_info, model_proc_output_info);
    meta_attacher = MetaAttacher::create(inference_type, inference_region, input_image_info);
}

OutputBlobs ConverterFacade::extractProcessedOutputBlobs(const OutputBlobs &all_output_blobs) const {
    if (all_output_blobs.empty())
        throw std::invalid_argument("Output blobs are empty.");

    // FIXME: c++17 has extract method for it
    OutputBlobs processed_output_blobs;
    for (const auto &blob : all_output_blobs) {
        if (layer_names_to_process.find(blob.first) != layer_names_to_process.cend())
            processed_output_blobs.insert(blob);
    }

    return processed_output_blobs;
}

void ConverterFacade::convert(const OutputBlobs &all_output_blobs, InferenceFrames &frames) const {
    const auto processed_output_blobs = extractProcessedOutputBlobs(all_output_blobs);
    auto tensors_batch = blob_to_meta->convert(processed_output_blobs);

    if (coordinates_restorer != nullptr)
        coordinates_restorer->restore(tensors_batch, frames);

    meta_attacher->attach(tensors_batch, frames);
}
