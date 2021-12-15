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

#include <algorithm>
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

            // get buffer and its size from classification_result
            const float *data = reinterpret_cast<const float *>(blob->GetData());
            if (not data)
                throw std::invalid_argument("Output blob data is nullptr");

            const size_t size = blob->GetSize();

            const auto &labels_raw = getLabels();
            if (labels_raw.empty()) {
                throw std::invalid_argument("Failed to get list of classification labels.");
            }

            for (size_t frame_index = 0; frame_index < batch_size; ++frame_index) {
                GVA::Tensor classification_result = createTensor();

                if (!raw_tesor_copying->enabled(RawTensorCopyingToggle::id))
                    CopyOutputBlobToGstStructure(blob, classification_result.gst_structure(),
                                                 BlobToMetaConverter::getModelName().c_str(), layer_name.c_str(),
                                                 batch_size, frame_index);

                // FIXME: then c++17 avaliable
                const auto item = get_data_by_batch_index(data, size, batch_size, frame_index);
                const float *item_data = item.first;
                const size_t item_data_size = item.second;

                if (!bIndex && labels_raw.size() != (bCompound ? 2 : 1) * item_data_size) {
                    throw std::invalid_argument("Wrong number of classification labels.");
                }

                if (bMax) {
                    auto max_elem = std::max_element(item_data, item_data + item_data_size);
                    auto index = std::distance(item_data, max_elem);
                    classification_result.set_string("label", labels_raw.at(index));
                    classification_result.set_int("label_id", index);
                    classification_result.set_double("confidence", *max_elem);
                } else if (bCompound) {
                    std::string string;
                    double threshold = classification_result.has_field("threshold")
                                           ? classification_result.get_double("threshold")
                                           : 0.5;
                    double confidence = 0;
                    for (size_t j = 0; j < item_data_size; j++) {
                        std::string label;
                        if (item_data[j] >= threshold) {
                            label = labels_raw.at(j * 2);
                        } else if (item_data[j] > 0) {
                            label = labels_raw.at(j * 2 + 1);
                        }
                        if (!label.empty()) {
                            if (!string.empty() and !isspace(string.back()))
                                string += " ";
                            string += label;
                        }
                        if (item_data[j] >= confidence)
                            confidence = item_data[j];
                    }
                    classification_result.set_string("label", string);
                    classification_result.set_double("confidence", confidence);
                } else if (bIndex) {
                    std::string string;
                    int max_value = 0;
                    for (size_t j = 0; j < item_data_size; j++) {
                        int value = static_cast<int>(item_data[j]);
                        if (value < 0 || static_cast<size_t>(value) >= labels_raw.size())
                            break;
                        if (value > max_value)
                            max_value = value;
                        string += labels_raw.at(value);
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
                    for (size_t j = 0; j < item_data_size; j++) {
                        if (item_data[j] >= threshold) {
                            classification_result.set_string("label", labels_raw.at(j));
                            classification_result.set_double("confidence", confidence);
                        }
                        if (item_data[j] >= confidence)
                            confidence = item_data[j];
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
