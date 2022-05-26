/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "label.h"

#include "copy_blob_to_gststruct.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"
#include "tensor.h"

#include <gst/gst.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;
using namespace InferenceBackend;

namespace {
void max_method(const float *data, size_t size, const std::vector<std::string> &labels, GVA::Tensor &result) {
    auto max_elem = std::max_element(data, data + size);
    auto index = std::distance(data, max_elem);
    result.set_string("label", labels.at(index));
    result.set_int("label_id", index);
    result.set_double("confidence", *max_elem);
}

void soft_max_method(const float *data, size_t size, const std::vector<std::string> &labels, GVA::Tensor &result) {
    auto max_confidence = std::max_element(data, data + size);
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] = std::exp(data[i] - *max_confidence);
        sum += sftm_arr.at(i);
    }
    if (sum > 0) {
        for (size_t i = 0; i < size; ++i) {
            sftm_arr[i] /= sum;
        }
    }
    auto max_elem = std::max_element(sftm_arr.begin(), sftm_arr.end());
    auto index = std::distance(sftm_arr.begin(), max_elem);
    result.set_string("label", labels.at(index));
    result.set_int("label_id", index);
    result.set_double("confidence", *max_elem);
}

void compound_method(const float *data, size_t size, const std::vector<std::string> &labels, GVA::Tensor &result) {
    std::string result_label;
    double threshold = result.get_double("threshold", 0.5);
    double confidence = 0;
    for (size_t j = 0; j < size; j++) {
        std::string label;
        if (data[j] >= threshold) {
            label = labels.at(j * 2);
        } else if (data[j] > 0) {
            label = labels.at(j * 2 + 1);
        }
        if (!label.empty()) {
            if (!result_label.empty() and !isspace(result_label.back()))
                result_label += " ";
            result_label += label;
        }
        if (data[j] >= confidence)
            confidence = data[j];
    }
    result.set_string("label", result_label);
    result.set_double("confidence", confidence);
}

void index_method(const float *data, size_t size, const std::vector<std::string> &labels, GVA::Tensor &result) {
    std::string result_label;
    int max_value = 0;
    for (size_t j = 0; j < size; j++) {
        int value = safe_convert<int>(data[j]);
        if (value < 0 || static_cast<size_t>(value) >= labels.size())
            break;
        if (value > max_value)
            max_value = value;
        result_label += labels.at(value);
    }
    if (max_value) {
        result.set_string("label", result_label);
    }
}

LabelConverter::Method method_from_string(const std::string &method_string) {
    const std::map<std::string, LabelConverter::Method> method_to_string_map = {
        {"max", LabelConverter::Method::Max},
        {"softmax", LabelConverter::Method::SoftMax},
        {"compound", LabelConverter::Method::Compound},
        {"index", LabelConverter::Method::Index}};

    const auto found = method_to_string_map.find(method_string);
    if (found == method_to_string_map.cend())
        return LabelConverter::Method::Default;

    return found->second;
}
} // namespace

LabelConverter::LabelConverter(BlobToMetaConverter::Initializer initializer)
    : BlobToTensorConverter(std::move(initializer)) {
    if (!raw_tensor_copying->enabled(RawTensorCopyingToggle::id))
        GVA_WARNING("%s", RawTensorCopyingToggle::deprecation_message.c_str());

    GstStructure *s = getModelProcOutputInfo().get();
    const auto method_string = gst_structure_get_string(s, "method");
    if (!method_string) {
        GVA_WARNING("Failed to get 'method' from model proc. Using default method");
        _method = LabelConverter::Method::Default;
    } else {
        _method = method_from_string(method_string);
    }
}

TensorsTable LabelConverter::convert(const OutputBlobs &output_blobs) const {
    ITT_TASK(__FUNCTION__);
    TensorsTable tensors_table;
    try {
        const size_t batch_size = getModelInputImageInfo().batch_size;
        tensors_table.resize(batch_size);

        for (const auto &blob_iter : output_blobs) {
            OutputBlob::Ptr blob = blob_iter.second;
            if (not blob) {
                throw std::invalid_argument("Output blob is empty.");
            }
            const std::string layer_name = blob_iter.first;

            // get buffer and its size from classification_result
            const float *data = reinterpret_cast<const float *>(blob->GetData());
            if (not data)
                throw std::invalid_argument("Output blob data is nullptr");

            const size_t size = blob->GetSize();

            const auto &labels_raw = getLabels();
            if (labels_raw.empty()) {
                throw std::invalid_argument("Failed to get list of classification labels.");
            }
            auto labels_raw_size = labels_raw.size();

            for (size_t frame_index = 0; frame_index < batch_size; ++frame_index) {
                GVA::Tensor classification_result = createTensor();

                if (!raw_tensor_copying->enabled(RawTensorCopyingToggle::id))
                    CopyOutputBlobToGstStructure(blob, classification_result.gst_structure(),
                                                 BlobToMetaConverter::getModelName().c_str(), layer_name.c_str(),
                                                 batch_size, frame_index);

                // FIXME: then c++17 avaliable
                const auto item = get_data_by_batch_index(data, size, batch_size, frame_index);
                const float *item_data = item.first;
                const size_t item_data_size = item.second;

                if (_method != Method::Index &&
                    labels_raw_size > (_method == Method::Compound ? 2 : 1) * item_data_size) {
                    throw std::invalid_argument("Wrong number of classification labels.");
                }

                switch (_method) {
                case Method::SoftMax:
                    soft_max_method(item_data, item_data_size, labels_raw, classification_result);
                    break;
                case Method::Compound:
                    compound_method(item_data, item_data_size, labels_raw, classification_result);
                    break;
                case Method::Index:
                    index_method(item_data, item_data_size, labels_raw, classification_result);
                    break;
                case Method::Max:
                    max_method(item_data, item_data_size, labels_raw, classification_result);
                    break;
                default:
                    throw std::runtime_error("Unknown method for 'to label' converter");
                }

                /* tensor_id - In different GStreamer versions tensors_batch are attached to the buffer in a different
                 * order. */
                /* type - To identify classification tensors among others. */
                /* element_id - To identify model_instance_id. */
                gst_structure_set(classification_result.gst_structure(), "tensor_id", G_TYPE_INT,
                                  safe_convert<int>(frame_index), "type", G_TYPE_STRING, "classification_result", NULL);
                tensors_table[frame_index].push_back(classification_result.gst_structure());
            }
        }
    } catch (const std::exception &e) {
        GVA_ERROR("An error occurred in the label converter: %s", e.what());
    }

    return tensors_table;
}
