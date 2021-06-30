/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "to_label.h"

#include "copy_blob_to_gststruct.h"
#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"
#include "tensor.h"

#include <gst/gst.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace post_processing;
using namespace InferenceBackend;

TensorsTable ToLabelConverter::convert(const OutputBlobs &output_blobs) const {
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
            for (size_t frame_index = 0; frame_index < batch_size; ++frame_index) {
                GVA::Tensor classification_result = createTensor();

                if (!raw_tesor_copying->enabled(RawTensorCopyingToggle::id))
                    CopyOutputBlobToGstStructure(blob, classification_result.gst_structure(),
                                                 BlobToMetaConverter::getModelName().c_str(), layer_name.c_str(),
                                                 batch_size, frame_index);

                std::string method =
                    classification_result.has_field("method") ? classification_result.get_string("method") : "";
                bool bMax = method == "max";
                bool bCompound = method == "compound";
                bool bIndex = method == "index";

                const auto &blob = blob_iter.second;

                // get buffer and its size from classification_result
                const float *data = reinterpret_cast<const float *>(blob->GetData());
                if (not data)
                    throw std::invalid_argument("Output blob data is nullptr");

                size_t data_size = blob->GetSize();

                // TODO: create method as func to call it in depends of name
                if (!bMax && !bCompound && !bIndex)
                    bMax = true;

                auto labels_raw = getLabels();
                if (labels_raw.empty()) {
                    throw std::invalid_argument("Failed to get list of classification labels.");
                }

                if (!bIndex) {
                    if (labels_raw.size() > (bCompound ? 2 : 1) * data_size) {
                        throw std::invalid_argument("Wrong number of classification labels.");
                    }
                }
                if (bMax) {
                    int index;
                    float confidence;
                    find_max_element_index(data, labels_raw.size(), index, confidence);
                    classification_result.set_string("label", labels_raw[index]);
                    classification_result.set_int("label_id", index);
                    classification_result.set_double("confidence", confidence);
                } else if (bCompound) {
                    std::string string;
                    double threshold = classification_result.has_field("threshold")
                                           ? classification_result.get_double("threshold")
                                           : 0.5;
                    double confidence = 0;
                    for (guint j = 0; j < (labels_raw.size()) / 2; j++) {
                        std::string label;
                        if (data[j] >= threshold) {
                            label = labels_raw[j * 2];
                        } else if (data[j] > 0) {
                            label = labels_raw[j * 2 + 1];
                        }
                        if (!label.empty()) {
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
                    for (guint j = 0; j < data_size; j++) {
                        int value = (int)data[j];
                        if (value < 0 || (guint)value >= labels_raw.size())
                            break;
                        if (value > max_value)
                            max_value = value;
                        string += labels_raw[value];
                    }
                    if (max_value) {
                        classification_result.set_string("label", string);
                    }
                } else {
                    // FIXME: remove or move deadcode
                    double threshold = classification_result.has_field("threshold")
                                           ? classification_result.get_double("threshold")
                                           : 0.5;
                    double confidence = 0;
                    for (guint j = 0; j < labels_raw.size(); j++) {
                        if (data[j] >= threshold) {
                            classification_result.set_string("label", labels_raw[j]);
                            classification_result.set_double("confidence", confidence);
                        }
                        if (data[j] >= confidence)
                            confidence = data[j];
                    }
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
        GVA_ERROR(e.what());
    }

    return tensors_table;
}

void ToLabelConverter::find_max_element_index(const float *array, int len, int &index, float &value) const {
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
