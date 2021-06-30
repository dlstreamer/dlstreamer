/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/gst.h>

#include "post_processor.hpp"

#include "../../inference_elements/model_proc/model_proc_provider.h"
#include "inference_backend/logger.h"

#include <exception>
#include <inference_engine.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

struct RawBlob : public InferenceBackend::OutputBlob {
    using Ptr = std::shared_ptr<RawBlob>;

    const void *data;
    size_t byte_size;
    InferenceEngine::TensorDesc tensor_desc;

    RawBlob() = delete;
    ~RawBlob() = default;

    RawBlob(const void *data, size_t byte_size, const InferenceEngine::TensorDesc &tensor_desc)
        : data(data), byte_size(byte_size), tensor_desc(tensor_desc) {
    }

    const std::vector<size_t> &GetDims() const {
        return tensor_desc.getDims();
    }

    InferenceBackend::Blob::Layout GetLayout() const {
        return static_cast<InferenceBackend::Blob::Layout>((int)tensor_desc.getLayout());
    }

    InferenceBackend::Blob::Precision GetPrecision() const {
        return static_cast<InferenceBackend::Blob::Precision>((int)tensor_desc.getPrecision());
    }

    const void *GetData() const {
        return data;
    }

    size_t GetByteSize() const {
        return byte_size;
    }
};

std::set<std::string> getDeclaredLayers(const std::map<std::string, GstStructure *> &model_proc_outputs) {
    std::set<std::string> layers;

    for (const auto &model_proc_output : model_proc_outputs) {
        GstStructure *s = model_proc_output.second;
        if (s == nullptr)
            throw std::runtime_error("Can not get model_proc output information.");

        if (not gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names"))
            return layers;
        if (gst_structure_has_field(s, "layer_name") and gst_structure_has_field(s, "layer_names"))
            return layers;

        if (gst_structure_has_field(s, "layer_name") and not gst_structure_has_field(s, "layer_names")) {
            layers.emplace(gst_structure_get_string(s, "layer_name"));
        }

        if (gst_structure_has_field(s, "layer_names") and not gst_structure_has_field(s, "layer_name")) {
            GValueArray *arr = NULL;
            gst_structure_get_array(const_cast<GstStructure *>(s), "layer_names", &arr);
            if (arr and arr->n_values) {
                for (guint i = 0; i < arr->n_values; ++i)
                    layers.emplace(g_value_get_string(g_value_array_get_nth(arr, i)));
                g_value_array_free(arr);
            } else {
                throw std::runtime_error("\"layer_names\" array is null.");
            }
        }
    }

    return layers;
}
std::set<std::string> getDeclaredLayers(const ModelOutputsInfo &model_outputs_info) {
    std::set<std::string> layers;

    for (const auto &output : model_outputs_info) {
        layers.insert(output.first);
    }

    return layers;
}
} // namespace

PostProcessor::ModelProcOutputsValidationResult
PostProcessor::validateModelProcOutputs(const std::map<std::string, GstStructure *> &model_proc_outputs,
                                        const ModelOutputsInfo &model_outputs_info) const {
    const size_t procs_num = model_proc_outputs.size();
    if (procs_num == 0) {
        return PostProcessor::ModelProcOutputsValidationResult::USE_DEFAULT;
    }

    const auto proc_layers = getDeclaredLayers(model_proc_outputs);
    if (proc_layers.empty() and procs_num == 1) {
        return PostProcessor::ModelProcOutputsValidationResult::USE_DEFAULT;
    }

    if (proc_layers.empty() and procs_num > 1) {
        return PostProcessor::ModelProcOutputsValidationResult::FAIL;
    }

    const auto model_layers = getDeclaredLayers(model_outputs_info);

    for (const auto &proc_layer : proc_layers) {
        if (model_layers.find(proc_layer) == model_layers.cend()) {
            const std::string msg = proc_layer + " is not contained among model's output layers.";
            GVA_ERROR(msg.c_str());
            return PostProcessor::ModelProcOutputsValidationResult::FAIL;
        }
    }

    return PostProcessor::ModelProcOutputsValidationResult::OK;
}

PostProcessor::PostProcessor(const TensorCaps &tensor_caps, const std::string &model_proc_path) {
    try {
        ModelImageInputInfo input_image_info;
        input_image_info.width = tensor_caps.GetDimension(2);
        input_image_info.height = tensor_caps.GetDimension(1);
        input_image_info.batch_size = tensor_caps.GetBatchSize();
        // TODO: we need format for ROIs?
        // input_image_info.format = tensor_caps.GetFormat();
        /* TODO: get model name from meta */
        std::string model_name = "action_recognition";

        ModelProcProvider model_proc_provider;
        model_proc_provider.readJsonFile(model_proc_path);
        const std::map<std::string, GstStructure *> &model_proc_outputs = model_proc_provider.parseOutputPostproc();
        std::map<std::string, std::vector<std::string>> labels;
        for (auto proc : model_proc_outputs) {
            GValueArray *labels_g_array = nullptr;
            gst_structure_get_array(proc.second, "labels", &labels_g_array);
            gst_structure_remove_field(proc.second, "labels");

            std::vector<std::string> labels_vector;
            labels_vector.reserve(labels_g_array->n_values);
            for (guint i = 0; i < labels_g_array->n_values; ++i) {
                labels_vector.push_back(g_value_get_string(g_value_array_get_nth(labels_g_array, i)));
            }

            labels[proc.first] = labels_vector;
        }
        if (labels.empty())
            labels.insert(std::make_pair("ANY", std::vector<std::string>{}));

        ModelOutputsInfo model_outputs_info;
        model_outputs_info["data"] = {static_cast<size_t>(tensor_caps.GetBatchSize()),
                                      static_cast<size_t>(tensor_caps.GetChannels())};
        auto validation_result = validateModelProcOutputs(model_proc_outputs, model_outputs_info);

        switch (validation_result) {
        case PostProcessor::ModelProcOutputsValidationResult::USE_DEFAULT: {
            std::unordered_set<std::string> layer_names;
            layer_names.reserve(model_outputs_info.size());
            for (const auto &output_info : model_outputs_info) {
                layer_names.insert(output_info.first);
            }

            converters.emplace_back(ConverterFacade(layer_names, model_proc_outputs.cbegin()->second, input_image_info,
                                                    model_name, labels.at(model_proc_outputs.cbegin()->first)));
        } break;
        case PostProcessor::ModelProcOutputsValidationResult::OK: {
            for (const auto &model_proc_output : model_proc_outputs) {
                if (model_proc_output.second == nullptr) {
                    throw std::runtime_error("Can not get model_proc output information.");
                }

                converters.emplace_back(ConverterFacade(model_proc_output.second, input_image_info, model_name,
                                                        labels.at(model_proc_output.first)));
            }
        } break;

        default:
            throw std::runtime_error("Cannot create post-processor with current model-proc information for model: " +
                                     model_name);
        }

    } catch (const std::exception &e) {
        std::string err_msg = e.what();

        GVA_ERROR(err_msg.c_str());
    }
}

PostProcessor::ExitStatus PostProcessor::process(GstBuffer *buffer, const TensorCaps &tensor_caps) const {
    try {
        std::map<std::string, InferenceBackend::OutputBlob::Ptr> output_blobs;

        auto precision = static_cast<InferenceEngine::Precision::ePrecision>(tensor_caps.GetPrecision());
        auto layout = static_cast<InferenceEngine::Layout>(tensor_caps.GetLayout());
        InferenceEngine::TensorDesc tensor_desc(
            precision,
            {static_cast<size_t>(tensor_caps.GetBatchSize()), static_cast<size_t>(tensor_caps.GetChannels())}, layout);

        GstMapInfo info;
        if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            GVA_ERROR("Failed to map buffer for reading");
            return ExitStatus::FAIL;
        }

        output_blobs.emplace(std::move("data"), std::make_shared<RawBlob>(info.data, info.size, tensor_desc));

        gst_buffer_unmap(buffer, &info);

        for (const auto &converter : converters) {
            converter.convert(output_blobs, buffer);
        }
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
        return ExitStatus::FAIL;
    }

    return ExitStatus::SUCCESS;
}
