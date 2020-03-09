/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_utils.h"
#include <fstream>
#include <inference_backend/logger.h>
#include <sstream>
#include <string>

std::string CreateNestedErrorMsg(const std::exception &e, int level) {
    static std::string msg = "\n";
    msg += std::string(level, ' ') + e.what() + "\n";
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        CreateNestedErrorMsg(e, level + 1);
    }
    return msg;
}

std::vector<std::string> SplitString(const std::string &input, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

using namespace InferenceBackend;

int GetUnbatchedSizeInBytes(OutputBlob::Ptr blob, size_t batch_size) {
    const std::vector<size_t> &dims = blob->GetDims();
    // On some type of models, such as SSD, batch-size at the output layer may differ from the input layer in case of
    // reshape. In the topology of such models, there is a decrease in dimension on hidden layers, which causes the
    // batch size to be lost. To correctly calculate size of blob you need yo multiply all the dimensions and divide by
    // the batch size.
    int size = dims[0];
    for (size_t i = 1; i < dims.size(); ++i)
        size *= dims[i];
    size /= batch_size;

    switch (blob->GetPrecision()) {
    case OutputBlob::Precision::FP32:
        size *= sizeof(float);
        break;
    case OutputBlob::Precision::U8:
        break;
    }
    return size;
}

bool file_exists(const std::string &path) {
    return std::ifstream(path).good();
}

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id) {
    GstStructure *object_id = gst_video_region_of_interest_meta_get_param(meta, "object_id");
    return object_id && gst_structure_get_int(object_id, "id", id);
}

void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id) {
    GstStructure *object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
    gst_video_region_of_interest_meta_add_param(meta, object_id);
}