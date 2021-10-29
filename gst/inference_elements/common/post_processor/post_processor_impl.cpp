/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "post_processor.h"

#include "converters/to_roi/boxes_labels.h"
#include "converters/to_roi/detection_output.h"
#include "converters/to_tensor/raw_data_copy.h"

#include "inference_backend/logger.h"

#include <exception>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

using namespace post_processing;

void PostProcessorImpl::setDefaultConverter(GstStructure *model_proc_output, const ModelOutputsInfo &model_outputs,
                                            ConverterType converter_type) {
    if (model_proc_output == nullptr)
        throw std::runtime_error("Can not get model_proc output information.");

    if (gst_structure_has_field(model_proc_output, "converter"))
        return;

    switch (converter_type) {
    case ConverterType::TO_ROI: {
        if (BoxesLabelsConverter::isValidModelOutputs(model_outputs)) {
            gst_structure_set(model_proc_output, "converter", G_TYPE_STRING, BoxesLabelsConverter::getName().c_str(),
                              NULL);
        } else if (DetectionOutputConverter::isValidModelOutputs(model_outputs)) {
            gst_structure_set(model_proc_output, "converter", G_TYPE_STRING,
                              DetectionOutputConverter::getName().c_str(), NULL);
        } else {
            throw std::runtime_error("Failed to determine the default detection converter. "
                                     "Please specify it yourself in the 'model-proc' file.");
        }
    } break;
    case ConverterType::RAW:
    case ConverterType::TO_TENSOR: {
        gst_structure_set(model_proc_output, "converter", G_TYPE_STRING, RawDataCopyConverter::getName().c_str(), NULL);
    } break;
    default:
        throw std::runtime_error("Unknown inference type.");
    }
}

PostProcessorImpl::PostProcessorImpl(Initializer initializer) {
    try {
        if (initializer.use_default) {
            std::unordered_set<std::string> layer_names;
            layer_names.reserve(initializer.model_outputs.size());
            for (const auto &output_info : initializer.model_outputs) {
                layer_names.insert(output_info.first);
            }

            std::map<std::string, GstStructure *> model_proc_outputs;
            GstStructureUniquePtr model_proc_output_info(nullptr, gst_structure_free);

            if (initializer.output_processors.empty()) {
                model_proc_output_info.reset(gst_structure_new_empty(any_layer_name.c_str()));
                model_proc_outputs.insert(std::make_pair(any_layer_name, model_proc_output_info.get()));
            } else {
                model_proc_outputs = initializer.output_processors;
            }
            setDefaultConverter(model_proc_outputs.cbegin()->second, initializer.model_outputs,
                                initializer.converter_type);

            if (initializer.converter_type == ConverterType::TO_ROI) {
                gst_structure_set(model_proc_outputs.cbegin()->second, "confidence_threshold", G_TYPE_DOUBLE,
                                  initializer.threshold, NULL);
            }

            converters.emplace_back(layer_names, model_proc_outputs.cbegin()->second, initializer.converter_type,
                                    initializer.attach_type, initializer.image_info, initializer.model_outputs,
                                    initializer.model_name, initializer.labels.at(model_proc_outputs.cbegin()->first));
        } else {
            for (const auto &model_proc_output : initializer.output_processors) {
                if (model_proc_output.second == nullptr) {
                    throw std::runtime_error("Can not get model_proc output information.");
                }

                if (initializer.converter_type == ConverterType::TO_ROI) {
                    gst_structure_set(model_proc_output.second, "confidence_threshold", G_TYPE_DOUBLE,
                                      initializer.threshold, NULL);
                }

                converters.emplace_back(model_proc_output.second, initializer.converter_type, initializer.attach_type,
                                        initializer.image_info, initializer.model_outputs, initializer.model_name,
                                        initializer.labels.at(model_proc_output.first));
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create PostProcessorImpl"));
    }
}

PostProcessorImpl::ExitStatus PostProcessorImpl::process(const OutputBlobs &output_blobs, FramesWrapper &frames) const {
    try {
        for (const auto &converter : converters) {
            converter.convert(output_blobs, frames);
        }
    } catch (const std::exception &e) {
        GVA_ERROR("Post-processing error: %s", Utils::createNestedErrorMsg(e).c_str());
        return ExitStatus::FAIL;
    }

    return ExitStatus::SUCCESS;
}
