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
#include "gstgvaclassify.h"
#include "gva_base_inference.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "post_processors_util.h"
#include "region_of_interest.h"
#include "video_frame.h"

#include "post_processors.h"
#include "post_processors_util.h"

void copy_buffer_to_structure(GstStructure *structure, const void *buffer, int size) {
    ITT_TASK(__FUNCTION__);
    GVariant *v = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buffer, size, 1);
    gsize n_elem;
    gst_structure_set(structure, "data_buffer", G_TYPE_VARIANT, v, "data", G_TYPE_POINTER,
                      g_variant_get_fixed_array(v, &n_elem, 1), NULL);
}

namespace {

using namespace InferenceBackend;

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

bool TensorToLabel(GVA::Tensor &classification_result, const std::vector<float> &data) {
    ITT_TASK(__FUNCTION__);
    std::string method = classification_result.has_field("method") ? classification_result.get_string("method") : "";
    bool bMax = method == "max";
    bool bCompound = method == "compound";
    bool bIndex = method == "index";

    if (!bMax && !bCompound && !bIndex)
        bMax = true;

    if (!classification_result.has_field("labels"))
        return false;
    GValueArray *labels = classification_result.get_array("labels");
    if (!bIndex) {
        if (labels->n_values > (bCompound ? 2 : 1) * data.size()) {
            g_value_array_free(labels);
            return false;
        }
    }
    if (bMax) {
        int index;
        float confidence;
        find_max_element_index(data, labels->n_values, index, confidence);
        if (data[index] > 0) {
            const gchar *label = g_value_get_string(labels->values + index);
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
            if (label)
                string += label;
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
                classification_result.set_string("label", label);
                classification_result.set_double("confidence", confidence);
            }
            if (data[j] >= confidence)
                confidence = data[j];
        }
    }

    if (labels)
        g_value_array_free(labels);
    return true;
}

bool TensorToLabelMoviTL(GVA::Tensor &classification_result, const std::vector<float> &data) {
    ITT_TASK(__FUNCTION__);
    if (data.empty())
        return false;

    size_t size_in_bytes = data.size() * sizeof(float);

    std::vector<float> dequantized_data(size_in_bytes);

    // scale
    Dequantizer dequantizer(classification_result);

    auto original_data = reinterpret_cast<const uint8_t *>(data.data());
    for (size_t i = 0; i < size_in_bytes; i++) {
        dequantized_data[i] = dequantizer.dequantize(original_data[i]);
    }

    // softmax
    softMax(dequantized_data.data(), size_in_bytes);

    // now it's back to probability
    bool result = TensorToLabel(classification_result, dequantized_data);

    return result;
}

G_GNUC_END_IGNORE_DEPRECATIONS

bool TensorToText(GVA::Tensor &classification_result, const std::vector<float> &data) {
    ITT_TASK(__FUNCTION__);
    double scale = classification_result.has_field("tensor_to_text_scale")
                       ? classification_result.get_double("tensor_to_text_scale")
                       : 1.0;
    int precision = classification_result.has_field("tensor_to_text_precision")
                        ? classification_result.get_int("tensor_to_text_precision")
                        : 2;
    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision);
    for (size_t i = 0; i < data.size(); ++i) {
        if (i)
            stream << ", ";
        stream << data[i] * scale;
    }
    classification_result.set_string("label", stream.str());
    return true;
}

bool ConvertBlobToClassificationResults(GVA::Tensor &classification_result) {
    ITT_TASK(__FUNCTION__);
    // get buffer and its size from classification_result
    const std::vector<float> data = classification_result.data<float>();
    if (data.empty())
        return false;

    std::string converter =
        classification_result.has_field("converter") ? classification_result.get_string("converter") : "";

    static std::map<std::string, std::function<bool(GVA::Tensor &, const std::vector<float> &)>> do_conversion{
        {"tensor_to_label", TensorToLabel},
        {"attributes", TensorToLabel}, // GVA plugin R1.2 backward compatibility
        {"tensor_to_text", TensorToText},
        {"tensor2text", TensorToText},                  // GVA plugin R1.2 backward compatibility
        {"tensor_to_label_moviTL", TensorToLabelMoviTL} // for movi Tensorflow-lite based model
    };

    if (do_conversion.find(converter) == do_conversion.end()) { // Wrong converter set in model-proc file
        // if converter is set to empty string, we don't log it here: it was already logged previously one time
        if (converter == "")
            return false;

        std::string valid_converters = "";
        for (auto converter_iter = do_conversion.begin(); converter_iter != do_conversion.end(); ++converter_iter) {
            valid_converters += "\"" + converter_iter->first + "\"";
            if (std::next(converter_iter) != do_conversion.end()) // last element
                valid_converters += ", ";
        }
        // if converter is set to some non-empty string, we log error each frame
        GST_ERROR(
            "Unknown post proc converter: \"%s\". Please set \"converter\" field in model-proc file to one of the "
            "following values: %s",
            converter.c_str(), valid_converters.c_str());
        return false;
    }

    return do_conversion[converter](classification_result, data);
}

void ExtractClassificationResults(const std::map<std::string, OutputBlob::Ptr> &output_blobs,
                                  std::vector<InferenceFrame> frames,
                                  const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name) {
    ITT_TASK(__FUNCTION__);
    if (frames.empty())
        std::logic_error("Vector of frames is empty.");
    int batch_size = frames.begin()->gva_base_inference->batch_size;

    for (const auto &blob_iter : output_blobs) {
        const std::string &layer_name = blob_iter.first;
        OutputBlob::Ptr blob = blob_iter.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty during post processing. Cannot access null object.");

        const uint8_t *data = (const uint8_t *)blob->GetData();
        if (data == NULL) {
            throw std::runtime_error("Data returned from GetData() is empty.");
        }
        int size = GetUnbatchedSizeInBytes(blob, batch_size);
        int rank = (int)blob->GetDims().size();

        for (size_t b = 0; b < frames.size(); b++) {
            // find meta
            auto current_roi = &frames[b].roi;
            gint roi_id = 0;
            get_object_id(current_roi, &roi_id);

            GVA::VideoFrame all_video_frame(frames[b].buffer, frames[b].info);
            bool is_roi_found = false;

            for (auto &roi : all_video_frame.regions()) {
                auto *meta = roi.meta();
                gint meta_id = 0;
                get_object_id(meta, &meta_id);
                if (meta->x == current_roi->x && meta->y == current_roi->y && meta->w == current_roi->w &&
                    meta->h == current_roi->h && meta_id == roi_id) {
                    // append new structure to ROI meta's params
                    GstStructure *classification_result_structure = nullptr;
                    const auto &post_proc = model_proc.find(layer_name);
                    if (post_proc != model_proc.end()) {
                        classification_result_structure = gst_structure_copy(post_proc->second);
                    } else {
                        classification_result_structure = gst_structure_new_empty(("layer:" + layer_name).data());
                    }
                    copy_buffer_to_structure(classification_result_structure, data + b * size, size);
                    GVA::Tensor classification_result = roi.add_tensor(classification_result_structure);
                    classification_result.set_string("layer_name", layer_name);
                    classification_result.set_string("model_name", model_name);
                    classification_result.set_int("precision", static_cast<int>(blob->GetPrecision()));
                    classification_result.set_int("layout", static_cast<int>(blob->GetLayout()));
                    classification_result.set_int("rank", rank);

                    if (post_proc != model_proc.end()) {
                        ConvertBlobToClassificationResults(classification_result);
                    }

                    GstGvaClassify *gva_classify = (GstGvaClassify *)frames[b].gva_base_inference;
                    if (gva_classify->skip_classified_objects and meta_id > 0)
                        gva_classify->classification_history->UpdateROIParams(meta_id,
                                                                              classification_result.gst_structure());

                    is_roi_found = true;
                    break;
                }
            }
            if (!is_roi_found) {
                GST_DEBUG("Can't find ROI metadata");
                continue;
            }
        }
    }
} // namespace
} // anonymous namespace

PostProcFunction EXTRACT_CLASSIFICATION_RESULTS = ExtractClassificationResults;
