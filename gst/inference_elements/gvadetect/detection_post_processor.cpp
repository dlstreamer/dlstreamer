/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "detection_post_processor.h"
#include "gstgvadetect.h"

#include "../base/inference_impl.h"
#include "converters/converter.h"
#include "inference_backend/logger.h"

using namespace InferenceBackend;
using namespace DetectionPlugin;
using namespace Converters;

namespace {

ConverterUniquePtr createConverter(const GstStructure *output_model_proc_info = nullptr,
                                   const ModelInputInfo input_info = ModelInputInfo());

LayersInfoMap createLayersInfo(const InferenceImpl::Model &model);
LayersInfoMap::iterator findFirstMatch(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                       LayersInfoMap &layers_info);
LayersInfoMap::iterator findFirstMatchOrAppend(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                               LayersInfoMap &layers_info, std::string &layer_name);

ConverterUniquePtr createConverter(const GstStructure *output_model_proc_info, const ModelInputInfo input_info) {
    std::unique_ptr<Converter> converter;
    converter = ConverterUniquePtr(Converter::create(output_model_proc_info, input_info));
    if (!converter) {
        throw std::runtime_error("Could not initialize converter" +
                                 Converter::getConverterType(output_model_proc_info) +
                                 ". Please, check if 'converter' field in model-proc file is valid.");
    }
    return converter;
}

LayersInfoMap createLayersInfo(const InferenceImpl::Model &model) {
    LayersInfoMap layers_info;
    ModelInputInfo input_info;
    model.inference->GetModelImageInputInfo(input_info.width, input_info.height, input_info.batch_size,
                                            input_info.format, input_info.memory_type);

    for (const auto &item : model.output_processor_info) {
        const std::string &layer_name = item.first;
        GstStructure *output_model_proc_info = item.second;

        auto converter = createConverter(output_model_proc_info, input_info);
        auto labels_it = model.labels.find(layer_name);
        GValueArray *labels = labels_it != model.labels.end() ? labels_it->second : nullptr;

        layers_info.emplace(layer_name, LayerInfo(std::move(converter), labels, output_model_proc_info));
    }
    return layers_info;
}

LayersInfoMap::iterator findFirstMatch(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                       LayersInfoMap &layers_info) {
    auto layer_info_it = layers_info.end();
    for (const auto &layer_out : output_blobs) {
        layer_info_it = layers_info.find(layer_out.first);
        if (layer_info_it != layers_info.end())
            break;
    }
    return layer_info_it;
}

LayersInfoMap::iterator findFirstMatchOrAppend(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                               LayersInfoMap &layers_info, std::string &layer_name) {
    if (layers_info.size() == 1 and layers_info.cbegin()->first == "ANY") {
        GVA_DEBUG("\"layer_name\" has been not specified. Converter will be applied to all output blobs.");
        layer_name = output_blobs.begin()->first;
        return layers_info.begin();
    }
    auto layer_info_it = layers_info.end();
    if (layers_info.empty()) {
        std::tie(layer_info_it, std::ignore) = layers_info.emplace(output_blobs.cbegin()->first, LayerInfo());
    } else {
        layer_info_it = findFirstMatch(output_blobs, layers_info);
        if (layer_info_it == layers_info.end()) {
            throw std::runtime_error("The specified \"layer_name\" has been not found among existing outputs.");
        }
    }
    layer_name = layer_info_it->first;
    return layer_info_it;
}

} // namespace

LayerInfo::LayerInfo()
    : converter(createConverter()), labels(GValueArrayUniquePtr(nullptr, g_value_array_free)),
      output_model_proc_info(gst_structure_new_empty("detection"), gst_structure_free) {
    if (output_model_proc_info == nullptr)
        throw std::runtime_error("Could not construct empty GstStructure");
}

LayerInfo::LayerInfo(ConverterUniquePtr converter, const GValueArray *labels,
                     const GstStructure *output_model_proc_info)
    : converter(std::move(converter)), labels(copy(labels, g_value_array_copy), g_value_array_free),
      output_model_proc_info(copy(output_model_proc_info, gst_structure_copy), gst_structure_free) {
}

LayerInfo::LayerInfo(ConverterUniquePtr converter, GValueArrayUniquePtr labels,
                     GstStructureUniquePtr output_model_proc_info)
    : converter(std::move(converter)), labels(std::move(labels)),
      output_model_proc_info(std::move(output_model_proc_info)) {
}

DetectionPostProcessor::DetectionPostProcessor(const InferenceImpl *inference_impl) {
    if (inference_impl == nullptr)
        throw std::runtime_error("DetectionPostProcessor could not be initialized with empty InferenceImpl");
    auto &models = inference_impl->GetModels();
    if (models.size() == 0)
        return;
    if (models.size() > 1)
        throw std::runtime_error("Multimodels is not supported");
    layers_info = createLayersInfo(models.front());
    model_name = models.front().name;
}

PostProcessor::ExitStatus DetectionPostProcessor::process(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                                          std::vector<std::shared_ptr<InferenceFrame>> &frames) {
    ITT_TASK(__FUNCTION__);
    try {
        if (output_blobs.empty())
            throw std::invalid_argument("There are no output blobs");

        std::string layer_name = "ANY";
        auto layer_info_it = findFirstMatchOrAppend(output_blobs, layers_info, layer_name);

        auto detection_result = GstStructureUniquePtr(
            gst_structure_copy(layer_info_it->second.output_model_proc_info.get()), gst_structure_free);
        gst_structure_set_name(detection_result.get(), "detection");
        GValueArray *labels_raw = layer_info_it->second.labels.get();
        if (not detection_result.get())
            throw std::runtime_error("Failed to create GstStructure with 'detection' name");

        gst_structure_set(detection_result.get(), "layer_name", G_TYPE_STRING, layer_name.c_str(), "model_name",
                          G_TYPE_STRING, model_name.c_str(), NULL);

        GstGvaDetect *gva_detect = (GstGvaDetect *)frames[0]->gva_base_inference;
        if (not gva_detect)
            throw std::invalid_argument("gva_base_inference attached to inference frames is nullptr");

        bool status = layer_info_it->second.converter->process(output_blobs, frames, detection_result.get(),
                                                               gva_detect->threshold, labels_raw);
        if (!status)
            return PostProcessor::ExitStatus::FAIL;

        return PostProcessor::ExitStatus::SUCCESS;

    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract detection results"));
    }
    return PostProcessor::ExitStatus::FAIL;
}
