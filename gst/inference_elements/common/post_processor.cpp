/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "post_processor.h"

#include "gstgvadetect.h"
#include "gva_base_inference.h"
#include "inference_backend/logger.h"
#include "inference_impl.h"
#include "model_proc_provider.h"

#include <map>
#include <string>
#include <vector>

using namespace post_processing;

namespace {

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
            GValueArray *arr = nullptr;
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

PostProcessor::ModelProcOutputsValidationResult
validateModelProcOutputs(const std::map<std::string, GstStructure *> &model_proc_outputs,
                         const ModelOutputsInfo &model_outputs_info) {
    const size_t procs_num = model_proc_outputs.size();
    if (procs_num == 0) {
        return PostProcessor::ModelProcOutputsValidationResult::USE_DEFAULT;
    }

    const auto proc_layers = getDeclaredLayers(model_proc_outputs);
    if (proc_layers.empty() and procs_num == 1) {
        return PostProcessor::ModelProcOutputsValidationResult::USE_DEFAULT;
    }

    if (proc_layers.empty() and procs_num > 1) {
        GVA_ERROR("Number of declared output_postprocs more then 1, but layers are not defined.");
        return PostProcessor::ModelProcOutputsValidationResult::FAIL;
    }

    const auto model_layers = getDeclaredLayers(model_outputs_info);

    for (const auto &proc_layer : proc_layers) {
        if (model_layers.find(proc_layer) == model_layers.cend()) {
            GVA_ERROR("The '%s' is not contained among model's output layers.", proc_layer.c_str());
            return PostProcessor::ModelProcOutputsValidationResult::FAIL;
        }
    }

    return PostProcessor::ModelProcOutputsValidationResult::OK;
}

} /* anonymous namespace */

PostProcessor::PostProcessor(InferenceImpl *inference_impl, GvaBaseInference *base_inference) {
    auto inference_type = base_inference->type;
    auto inference_region = base_inference->inference_region;
    const std::string any_layer_name = "ANY";

    PostProcessorImpl::Initializer initializer;
    /* set model & its name */
    const auto &model = inference_impl->GetModel();
    initializer.model_name = model.name;
    /* set output labels */
    initializer.labels = model.labels;
    if (initializer.labels.empty())
        initializer.labels.insert(std::make_pair(any_layer_name, std::vector<std::string>{}));
    /* set input image info */
    model.inference->GetModelImageInputInfo(initializer.image_info.width, initializer.image_info.height,
                                            initializer.image_info.batch_size, initializer.image_info.format,
                                            initializer.image_info.memory_type);
    /* set outputs info */
    initializer.model_outputs = model.inference->GetModelOutputsInfo();
    /* set output processors from model proc */
    initializer.output_processors = model.output_processor_info;
    /* validate outputs */
    auto validation_result = validateModelProcOutputs(initializer.output_processors, initializer.model_outputs);
    if (validation_result == ModelProcOutputsValidationResult::FAIL) {
        throw std::runtime_error("Cannot create post-processor with current model-proc information for model: " +
                                 initializer.model_name);
    }
    initializer.use_default = validation_result == ModelProcOutputsValidationResult::USE_DEFAULT;
    /* set attach type */
    if (inference_region == InferenceRegionType::FULL_FRAME) {
        initializer.attach_type = AttachType::TO_FRAME;
    } else if (inference_region == InferenceRegionType::ROI_LIST) {
        initializer.attach_type = AttachType::TO_ROI;
    }
    /* set converter type & threshold for detection */
    if (inference_type == InferenceType::GST_GVA_DETECT_TYPE) {
        initializer.converter_type = ConverterType::TO_ROI;
        GstGvaDetect *gva_detect = reinterpret_cast<GstGvaDetect *>(base_inference);
        initializer.threshold = gva_detect->threshold;
    } else if (inference_type == InferenceType::GST_GVA_CLASSIFY_TYPE) {
        initializer.converter_type = ConverterType::TO_TENSOR;
    } else if (inference_type == InferenceType::GST_GVA_INFERENCE_TYPE) {
        initializer.converter_type = ConverterType::RAW;
    }
    new (&post_proc_impl) PostProcessorImpl(std::move(initializer));
}

PostProcessor::PostProcessor(size_t image_width, size_t image_height, size_t batch_size, const std::string &model_proc,
                             const std::string &model_name, const ModelOutputsInfo &tensor_descs,
                             ConverterType converter_type, double threshold) {

    post_processing::PostProcessorImpl::Initializer initializer;
    const std::string any_layer_name = "ANY";
    /* image info */
    initializer.image_info.width = image_width;
    initializer.image_info.height = image_height;
    initializer.image_info.batch_size = batch_size;
    /* model proc info: labels & output processors */
    if (!model_proc.empty()) {
        ModelProcProvider model_proc_provider;
        model_proc_provider.readJsonFile(model_proc);
        initializer.output_processors = model_proc_provider.parseOutputPostproc();
    }
    for (const auto &proc : initializer.output_processors) {
        GValueArray *labels_g_array = nullptr;
        gst_structure_get_array(proc.second, "labels", &labels_g_array);
        gst_structure_remove_field(proc.second, "labels");

        std::vector<std::string> labels_vector;
        if (labels_g_array) {
            labels_vector.reserve(labels_g_array->n_values);
            for (auto i = 0u; i < labels_g_array->n_values; ++i) {
                labels_vector.push_back(g_value_get_string(g_value_array_get_nth(labels_g_array, i)));
            }
        }

        initializer.labels[proc.first] = labels_vector;
    }
    if (initializer.labels.empty())
        initializer.labels.insert(std::make_pair(any_layer_name, std::vector<std::string>{}));
    /* model info: name & outputs */
    initializer.model_name = model_name;
    initializer.model_outputs = tensor_descs;
    /* validate outputs */
    auto validation_result = validateModelProcOutputs(initializer.output_processors, initializer.model_outputs);
    if (validation_result == ModelProcOutputsValidationResult::FAIL) {
        throw std::runtime_error("Cannot create post-processor with current model-proc information for model: " +
                                 initializer.model_name);
    }
    initializer.use_default = validation_result == ModelProcOutputsValidationResult::USE_DEFAULT;
    /* other */
    initializer.threshold = threshold;
    initializer.attach_type = AttachType::FOR_MICRO;
    initializer.converter_type = converter_type;
    new (&post_proc_impl) PostProcessorImpl(std::move(initializer));
}

PostProcessorImpl::ExitStatus PostProcessor::process(const OutputBlobs &blobs, InferenceFrames &frames) const {
    auto frame_wrappers = FramesWrapper(frames);
    return post_proc_impl.process(blobs, frame_wrappers);
}
PostProcessorImpl::ExitStatus PostProcessor::process(GstBuffer *buffer,
                                                     const std::vector<TensorDesc> &output_tensors_descs,
                                                     const std::string &instance_id) const {
    try {
        OutputBlobs output_blobs;

        GstMapInfo info;
        if (!gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            GVA_ERROR("Failed to map buffer for reading");
            return PostProcessorImpl::ExitStatus::FAIL;
        }

        size_t offset = 0;
        for (const auto &e : output_tensors_descs) {
            output_blobs.emplace(e.name, std::make_shared<RawBlob>(info.data + offset, e.size, e.ie_desc));
            offset = safe_add(offset, e.size);
        }
        gst_buffer_unmap(buffer, &info);

        auto frame_wrappers = FramesWrapper(buffer, instance_id);
        return post_proc_impl.process(output_blobs, frame_wrappers);
    } catch (const std::exception &e) {
        GVA_ERROR("An error occurred while post-processing: %s", e.what());
        return PostProcessorImpl::ExitStatus::FAIL;
    }
}
