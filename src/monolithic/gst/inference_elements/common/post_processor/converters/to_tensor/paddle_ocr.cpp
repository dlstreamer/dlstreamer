/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "paddle_ocr.h"
#include "copy_blob_to_gststruct.h"
#include "inference_backend/logger.h"
#include "safe_arithmetic.hpp"
#include <algorithm>
#include <cmath>
#include <gst/gst.h>
#include <sstream>
#include <stdexcept>

#include <fstream>
#include <iostream>

using namespace post_processing;
using namespace InferenceBackend;

// Constructor to initialize the OCRConverter with the initializer.
PaddleOCRConverter::PaddleOCRConverter(BlobToMetaConverter::Initializer initializer)
    : BlobToTensorConverter(std::move(initializer)) {
}

TensorsTable PaddleOCRConverter::convert(const OutputBlobs &output_blobs) {
    ITT_TASK(__FUNCTION__);
    TensorsTable tensors_table;

    try {
        const size_t batch_size = getModelInputImageInfo().batch_size;
        tensors_table.resize(batch_size);

        for (const auto &blob_iter : output_blobs) {
            OutputBlob::Ptr blob = blob_iter.second;
            if (!blob) {
                throw std::invalid_argument("Output blob is empty");
            }

            const float *data = reinterpret_cast<const float *>(blob->GetData());
            if (!data) {
                throw std::invalid_argument("Output blob data is nullptr");
            }

            const size_t data_size = blob->GetSize();
            const std::string layer_name = blob_iter.first;

            for (size_t batch_elem_index = 0; batch_elem_index < batch_size; ++batch_elem_index) {
                GVA::Tensor classification_result = createTensor();

                if (!raw_tensor_copying->enabled(RawTensorCopyingToggle::id))
                    CopyOutputBlobToGstStructure(blob, classification_result.gst_structure(),
                                                 BlobToMetaConverter::getModelName().c_str(), layer_name.c_str(),
                                                 batch_size, batch_elem_index);

                const auto item = get_data_by_batch_index(data, data_size, batch_size, batch_elem_index);
                const float *item_data = item.first;

                std::string decoded_text = decodeOutputTensor(item_data);

                if (decoded_text.size() > SEQ_MINLEN)
                    classification_result.set_string("label", decoded_text);
                else
                    classification_result.set_string("label", "");

                // Set metadata for the tensor in the GstStructure
                gst_structure_set(classification_result.gst_structure(), "tensor_id", G_TYPE_INT,
                                  safe_convert<int>(batch_elem_index), "type", G_TYPE_STRING, "classification_result",
                                  NULL);
                std::vector<GstStructure *> tensors{classification_result.gst_structure()};
                tensors_table[batch_elem_index].push_back(tensors);
            }
        }
    } catch (const std::exception &e) {
        GVA_ERROR("An error occurred in OCR converter: %s", e.what());
    }

    return tensors_table;
}

// Function to decode output tensor into text using the charset
std::string PaddleOCRConverter::decodeOutputTensor(const float *item_data) {

    std::vector<int> pred_indices(SEQUENCE_LENGTH); // Stores indices of max elements for each sequence

    for (size_t i = 0; i < SEQUENCE_LENGTH; ++i) {
        const float *row_start = item_data + i * CHARSET_LEN; // Pointer to the start of the current sequence
        const float *max_element_ptr = std::max_element(row_start, row_start + CHARSET_LEN); // Find max element
        int max_index = std::distance(row_start, max_element_ptr); // Calculate index of max element
        pred_indices[i] = max_index;                               // Store the index
    }

    // Decode the indices into text using the charset
    return decode(pred_indices);
}

// Function to decode text indices into text labels using a charset
std::string PaddleOCRConverter::decode(const std::vector<int> &text_index) {

    std::string char_list;                 // Accumulates characters for the sequence
    std::vector<int> ignored_tokens = {0}; // Tokens to ignore during decoding

    // Iterate over each index in the sequence
    for (size_t idx = 0; idx < text_index.size(); ++idx) {
        int current_index = text_index[idx];

        // Skip ignored tokens
        if (std::find(ignored_tokens.begin(), ignored_tokens.end(), current_index) != ignored_tokens.end()) {
            continue;
        }

        // Remove consecutive duplicate indices (optional)
        if (idx > 0 && text_index[idx - 1] == current_index) {
            continue;
        }

        if (current_index >= 0 && current_index < (int)CHARACTER_SET.size()) {
            char_list.append(CHARACTER_SET[current_index]);
        }
    }

    return char_list; // Return the decoded text
}
