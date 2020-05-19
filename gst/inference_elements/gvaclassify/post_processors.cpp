/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "classification_history.h"
#include "copy_blob_to_gststruct.h"
#include "gstgvaclassify.h"
#include "gva_base_inference.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "region_of_interest.h"
#include "video_frame.h"

#include "post_processors.h"
#include "post_processors_util.h"

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

void TensorToLabel(GVA::Tensor &classification_result) {
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

        if (!classification_result.has_field("labels"))
            throw std::invalid_argument("List of classification labels is not set");

        GValueArray *labels_raw = nullptr;
        gst_structure_get_array(classification_result.gst_structure(), "labels", &labels_raw);
        if (not labels_raw)
            throw std::invalid_argument("Failed to get list of classification labels");

        auto labels = std::unique_ptr<GValueArray, decltype(&g_value_array_free)>(labels_raw, g_value_array_free);

        if (!bIndex) {
            if (labels->n_values > (bCompound ? 2 : 1) * data.size()) {
                throw std::invalid_argument("Wrong number of classification labels");
            }
        }
        if (bMax) {
            int index;
            float confidence;
            find_max_element_index(data, labels->n_values, index, confidence);
            if (data[index] > 0) {
                const gchar *label = g_value_get_string(labels->values + index);
                if (label)
                    classification_result.set_string("label", label);
                classification_result.set_int("label_id", index);
                classification_result.set_double("confidence", confidence);
            }
        } else if (bCompound) {
            std::string string;
            double threshold =
                classification_result.has_field("threshold") ? classification_result.get_double("threshold") : 0.5;
            double confidence = 0;
            for (guint j = 0; j < (labels->n_values) / 2; j++) {
                const gchar *label = NULL;
                if (data[j] >= threshold) {
                    label = g_value_get_string(labels->values + j * 2);
                } else if (data[j] > 0) {
                    label = g_value_get_string(labels->values + j * 2 + 1);
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
                if (value < 0 || (guint)value >= labels->n_values)
                    break;
                if (value > max_value)
                    max_value = value;
                string += g_value_get_string(labels->values + value);
            }
            if (max_value) {
                classification_result.set_string("label", string);
            }
        } else {
            double threshold =
                classification_result.has_field("threshold") ? classification_result.get_double("threshold") : 0.5;
            double confidence = 0;
            for (guint j = 0; j < labels->n_values; j++) {
                if (data[j] >= threshold) {
                    const gchar *label = g_value_get_string(labels->values + j);
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

void TensorToText(GVA::Tensor &classification_result) {
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

void ConvertBlobToClassificationResults(GVA::Tensor &classification_result) {
    ITT_TASK(__FUNCTION__);

    std::string converter =
        classification_result.has_field("converter") ? classification_result.get_string("converter") : "";

    static std::map<std::string, std::function<void(GVA::Tensor &)>> do_conversion{
        {"tensor_to_label", TensorToLabel},
        {"attributes", TensorToLabel}, // GVA plugin R1.2 backward compatibility
        {"tensor_to_text", TensorToText},
        {"tensor2text", TensorToText} // GVA plugin R1.2 backward compatibility
    };

    if (do_conversion.find(converter) == do_conversion.end()) { // Wrong converter set in model-proc file
        // if converter is set to empty string, we don't log it here: it was already logged previously one time
        if (converter == "") {
            GVA_WARNING("No classification post-processing converter set");
            return;
        }

        std::string valid_converters = "";
        for (auto converter_iter = do_conversion.begin(); converter_iter != do_conversion.end(); ++converter_iter) {
            valid_converters += "\"" + converter_iter->first + "\"";
            if (std::next(converter_iter) != do_conversion.end()) // last element
                valid_converters += ", ";
        }
        throw std::invalid_argument(
            "Unknown post processing converter set: '" + converter +
            "'. Please set 'converter' field in model-proc file to one of the following values: " + valid_converters);
    }
    do_conversion[converter](classification_result);
}

void ExtractClassificationResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                  std::vector<InferenceFrame> frames,
                                  const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name) {
    ITT_TASK(__FUNCTION__);
    try {
        if (frames.empty())
            throw std::invalid_argument("There are no inference frames");

        for (const auto &blob_iter : output_blobs) {
            const std::string &layer_name = blob_iter.first;
            OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is empty");

            for (size_t b = 0; b < frames.size(); b++) {
                auto current_roi = &frames[b].roi;
                GstBuffer *buffer = frames[b].buffer;
                if (not buffer)
                    throw std::invalid_argument("Inference frame's buffer is nullptr");

                // find meta
                bool is_roi_found = false;
                GstVideoRegionOfInterestMeta *meta = NULL;
                gpointer state = NULL;
                while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
                    if (meta->x == current_roi->x && meta->y == current_roi->y && meta->w == current_roi->w &&
                        meta->h == current_roi->h && meta->roi_type == current_roi->roi_type) {

                        // append new structure to ROI meta's params
                        GstStructure *classification_result_structure = nullptr;
                        const auto &post_proc = model_proc.find(layer_name);
                        if (post_proc != model_proc.end()) {
                            classification_result_structure = gst_structure_copy(post_proc->second);
                        } else {
                            classification_result_structure = gst_structure_new_empty(("layer:" + layer_name).data());
                        }
                        if (not classification_result_structure)
                            throw std::runtime_error("Failed to create classification result tensor");

                        CopyOutputBlobToGstStructure(blob, classification_result_structure, model_name,
                                                     layer_name.c_str(), frames.size(), b);

                        gst_video_region_of_interest_meta_add_param(meta, classification_result_structure);
                        GVA::Tensor classification_result(classification_result_structure);

                        if (post_proc != model_proc.end()) {
                            ConvertBlobToClassificationResults(classification_result);
                        }
                        GstGvaClassify *gva_classify = (GstGvaClassify *)frames[b].gva_base_inference;
                        gint meta_id = 0;
                        get_object_id(meta, &meta_id);
                        if (gva_classify->reclassify_interval != 1 and meta_id > 0)
                            gva_classify->classification_history->UpdateROIParams(meta_id,
                                                                                  classification_result_structure);
                        is_roi_found = true;
                        break;
                    }
                }
                if (not is_roi_found) {
                    GST_WARNING("No detection tensors were found for this buffer");
                    continue;
                }
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract classification results"));
    }
} // namespace
} // anonymous namespace

PostProcFunction EXTRACT_CLASSIFICATION_RESULTS = ExtractClassificationResults;
