/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "classification_post_processors.h"

#include "classification_history.h"
#include "copy_blob_to_gststruct.h"

#include "inference_impl.h"

#include "gva_base_inference.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "region_of_interest.h"
#include "video_frame.h"

#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

using namespace ClassificationPlugin;
using namespace InferenceBackend;

namespace {

static void find_max_element_index(const std::vector<float> &array, int len, int &index, float &value) {
    ITT_TASK(__FUNCTION__);
    index = 0;
    value = array[0];
    for (int i = 1; i < len; i++) {
        if (array[i] > value) {
            index = i;
            value = array[i];
        }
    }
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

void TensorToLabel(GVA::Tensor &classification_result, GValueArray *labels_raw) {
    ITT_TASK(__FUNCTION__);
    try {
        std::string method =
            classification_result.has_field("method") ? classification_result.get_string("method") : "";
        bool bMax = method == "max";
        bool bCompound = method == "compound";
        bool bIndex = method == "index";
        // get buffer and its size from classification_result
        const std::vector<float> data = classification_result.data<float>();
        if (data.empty())
            throw std::invalid_argument("Failed to get classification tensor raw data");

        if (!bMax && !bCompound && !bIndex)
            bMax = true;

        if (labels_raw == nullptr) {
            throw std::invalid_argument("Failed to get list of classification labels");
        }

        if (!bIndex) {
            if (labels_raw->n_values > (bCompound ? 2 : 1) * data.size()) {
                throw std::invalid_argument("Wrong number of classification labels");
            }
        }
        if (bMax) {
            int index;
            float confidence;
            find_max_element_index(data, labels_raw->n_values, index, confidence);
            const gchar *label = g_value_get_string(labels_raw->values + index);
            if (label)
                classification_result.set_string("label", label);
            classification_result.set_int("label_id", index);
            classification_result.set_double("confidence", confidence);
        } else if (bCompound) {
            std::string string;
            double threshold =
                classification_result.has_field("threshold") ? classification_result.get_double("threshold") : 0.5;
            double confidence = 0;
            for (guint j = 0; j < (labels_raw->n_values) / 2; j++) {
                const gchar *label = NULL;
                if (data[j] >= threshold) {
                    label = g_value_get_string(labels_raw->values + j * 2);
                } else if (data[j] > 0) {
                    label = g_value_get_string(labels_raw->values + j * 2 + 1);
                }
                if (label) {
                    if (!string.empty() and !isspace(string.back()))
                        string += " ";
                    string += label;
                }
                if (data[j] >= confidence)
                    confidence = data[j];
            }
            classification_result.set_string("label", string);
            classification_result.set_double("confidence", confidence);
        } else if (bIndex) {
            std::string string;
            int max_value = 0;
            for (guint j = 0; j < data.size(); j++) {
                int value = (int)data[j];
                if (value < 0 || (guint)value >= labels_raw->n_values)
                    break;
                if (value > max_value)
                    max_value = value;
                string += g_value_get_string(labels_raw->values + value);
            }
            if (max_value) {
                classification_result.set_string("label", string);
            }
        } else {
            double threshold =
                classification_result.has_field("threshold") ? classification_result.get_double("threshold") : 0.5;
            double confidence = 0;
            for (guint j = 0; j < labels_raw->n_values; j++) {
                if (data[j] >= threshold) {
                    const gchar *label = g_value_get_string(labels_raw->values + j);
                    if (label)
                        classification_result.set_string("label", label);
                    classification_result.set_double("confidence", confidence);
                }
                if (data[j] >= confidence)
                    confidence = data[j];
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do tensor to label post-processing"));
    }
}

G_GNUC_END_IGNORE_DEPRECATIONS

void TensorToText(GVA::Tensor &classification_result, GValueArray * /*labels*/) {
    ITT_TASK(__FUNCTION__);
    try {
        // get buffer and its size from classification_result
        const std::vector<float> data = classification_result.data<float>();
        if (data.empty())
            throw std::invalid_argument("Failed to get classification tensor raw data");

        double scale = classification_result.get_double("tensor_to_text_scale", 1.0);
        int precision = classification_result.get_int("tensor_to_text_precision", 2);
        std::stringstream stream;
        stream << std::fixed << std::setprecision(precision);
        for (size_t i = 0; i < data.size(); ++i) {
            if (i)
                stream << ", ";
            stream << data[i] * scale;
        }
        classification_result.set_string("label", stream.str());
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do tensor to text post-processing"));
    }
}

using ConvertersMap = std::map<std::string, ConverterFunctionType>;

std::string getConverterName(const GVA::Tensor &tensor_meta) {
    std::string converter_name("raw_data_copy");
    if (tensor_meta.has_field("converter")) {
        converter_name = tensor_meta.get_string("converter");
    } else {
        GVA_DEBUG("No classification post-processing converter is set");
    }

    return converter_name;
}

ConverterFunctionType getConverter(GstStructure *model_proc_info) {
    ITT_TASK(__FUNCTION__);
    if (model_proc_info == nullptr)
        throw std::invalid_argument("Model proc is empty");
    GVA::Tensor classification_result(model_proc_info);
    std::string converter_name = getConverterName(classification_result);

    static ConvertersMap converters{{"tensor_to_label", TensorToLabel},
                                    {"attributes", TensorToLabel}, // GVA plugin R1.2 backward compatibility
                                    {"tensor_to_text", TensorToText},
                                    {"tensor2text", TensorToText}, // GVA plugin R1.2 backward compatibility,
                                    {"raw_data_copy", [](GVA::Tensor &, GValueArray *) {
                                         // raw data copying will happen anyway
                                     }}};

    auto converter_it = converters.find(converter_name);
    if (converter_it == converters.end()) { // Wrong converter set in model-proc file
        std::string valid_converters = std::accumulate(
            converters.begin(), converters.end(), std::string(""),
            [](std::string acc, ConvertersMap::value_type &converter) { return std::move(acc) + converter.first; });

        throw std::invalid_argument(
            "Unknown post processing converter set: '" + converter_name +
            "'. Please set 'converter' field in model-proc file to one of the following values: " + valid_converters);
    }
    return converter_it->second;
}

GstStructure *createResultStructure(OutputBlob::Ptr &blob, ClassificationLayerInfo &info, const std::string &model_name,
                                    const std::string &layer_name, size_t batch_size, size_t frame_index) {
    GstStructure *classification_result = nullptr;
    if (info.model_proc_info)
        classification_result = copy(info.model_proc_info.get(), gst_structure_copy);
    else
        throw std::runtime_error("Failed to create classification result structure: model-proc is null.");

    if (!classification_result)
        throw std::runtime_error("Failed to create classification result tensor");

    CopyOutputBlobToGstStructure(blob, classification_result, model_name.c_str(), layer_name.c_str(), batch_size,
                                 frame_index);

    GVA::Tensor wrapped_tensor(classification_result);

    if (info.converter) {
        info.converter(wrapped_tensor, info.labels.get());
    }
    return classification_result;
}

inline bool sameRegion(GstVideoRegionOfInterestMeta *left, GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

inline GstVideoRegionOfInterestMeta *findDetectionMeta(InferenceFrame *frame) {
    GstBuffer *buffer = frame->buffer;
    if (not buffer)
        throw std::invalid_argument("Inference frame's buffer is nullptr");
    auto frame_roi = &frame->roi;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return nullptr;
}

ClassificationLayersInfoMap createClassificationLayersInfoMap(const InferenceImpl::Model &model) {
    ClassificationLayersInfoMap layers_info;
    for (const auto &item : model.output_processor_info) {
        const std::string &layer_name = item.first;
        GstStructure *model_proc_info = item.second;

        auto converter = getConverter(model_proc_info);
        auto labels_it = model.labels.find(layer_name);
        GValueArray *labels = labels_it != model.labels.end() ? labels_it->second : nullptr;

        layers_info.emplace(layer_name, ClassificationLayerInfo(std::move(converter), labels, model_proc_info));
    }
    return layers_info;
}

} // anonymous namespace

ClassificationLayerInfo::ClassificationLayerInfo()
    : converter(), labels(nullptr, g_value_array_free), model_proc_info(nullptr, gst_structure_free) {
}

ClassificationLayerInfo::ClassificationLayerInfo(const std::string &layer_name)
    : converter(), labels(nullptr, g_value_array_free),
      model_proc_info(gst_structure_new_empty(("layer:" + layer_name).c_str()), gst_structure_free) {
}

ClassificationLayerInfo::ClassificationLayerInfo(ConverterFunctionType converter, const GValueArray *labels,
                                                 const GstStructure *model_proc_info)
    : converter(converter), labels(copy(labels, g_value_array_copy), g_value_array_free),
      model_proc_info(copy(model_proc_info, gst_structure_copy), gst_structure_free) {
}

ClassificationLayerInfo::ClassificationLayerInfo(ConverterFunctionType converter, GValueArrayUniquePtr labels,
                                                 GstStructureUniquePtr model_proc_info)
    : converter(converter), labels(std::move(labels)), model_proc_info(std::move(model_proc_info)) {
}

ClassificationPostProcessor::ClassificationPostProcessor(const InferenceImpl *inference_impl) {
    if (inference_impl == nullptr)
        throw std::runtime_error("ClassificationPostProcessor could not be initialized with empty InferenceImple");
    auto &models = inference_impl->GetModels();
    if (models.size() == 0)
        return;
    if (models.size() > 1)
        throw std::runtime_error("Multimodels is not supported");
    layers_info = createClassificationLayersInfoMap(models.front());
    model_name = models.front().name;
}

void ClassificationPostProcessor::fillLayersInfoIfEmpty(
    const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) {
    if (layers_info.empty()) {
        for (const auto &blob_iter : output_blobs) {
            const std::string &layer_name = blob_iter.first;
            layers_info.emplace(layer_name, ClassificationLayerInfo(layer_name));
        }
    }
}

PostProcessor::ExitStatus ClassificationPostProcessor::pushClassificationResultToFrames(
    OutputBlob::Ptr &blob, ClassificationLayerInfo &layer_info, const std::string &layer_name,
    std::vector<std::shared_ptr<InferenceFrame>> &frames) {
    if (not blob)
        throw std::invalid_argument("Output blob is empty");

    for (size_t frame_index = 0; frame_index < frames.size(); frame_index++) {
        GstVideoRegionOfInterestMeta *meta = findDetectionMeta(frames[frame_index].get());
        if (meta) {
            auto classification_result =
                createResultStructure(blob, layer_info, model_name, layer_name, frames.size(), frame_index);
            if (!classification_result)
                return PostProcessor::ExitStatus::FAIL;

            gst_video_region_of_interest_meta_add_param(meta, classification_result);
            // store classifications to update classification history when pushing output buffers
            frames[frame_index]->roi_classifications.push_back(classification_result);
        } else {
            GST_WARNING("No detection tensors were found for this buffer");
        }
    }
    return PostProcessor::ExitStatus::SUCCESS;
}

PostProcessor::ExitStatus
ClassificationPostProcessor::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                     std::vector<std::shared_ptr<InferenceFrame>> &frames) {
    ITT_TASK(__FUNCTION__);
    auto exec_status = PostProcessor::ExitStatus::FAIL;
    try {
        if (frames.empty())
            throw std::invalid_argument("There are no inference frames");

        // If model-proc has not been set, then layers_info will be initialized by default Converter for all layers
        // from output_blobs just on first frame.
        fillLayersInfoIfEmpty(output_blobs);

        if (layers_info.size() == 1 and layers_info.cbegin()->first == "ANY") {
            GVA_DEBUG("\"layer_name\" has been not specified. Converter will be applied to all output blobs.");

            auto &layer_info = layers_info.begin()->second;

            for (const auto &blob_iter : output_blobs) {
                const std::string &layer_name = blob_iter.first;

                OutputBlob::Ptr blob = blob_iter.second;
                exec_status = pushClassificationResultToFrames(blob, layer_info, layer_name, frames);
            }
        } else {
            for (auto &layer_info_it : layers_info) {
                const auto &layer_name = layer_info_it.first;
                auto &layer_info = layer_info_it.second;

                const auto blob_iter = output_blobs.find(layer_name);
                if (blob_iter == output_blobs.cend()) {
                    throw std::runtime_error("The specified \"layer_name\" has been not found among existing outputs.");
                }

                OutputBlob::Ptr blob = blob_iter->second;
                exec_status = pushClassificationResultToFrames(blob, layer_info, layer_name, frames);
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract classification results"));
    }
    return exec_status;
}
