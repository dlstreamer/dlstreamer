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
    if (dims[0] != batch_size) {
        throw std::logic_error("Blob last dimension should be equal to batch size");
    }
    int size = dims[1];
    for (size_t i = 2; i < dims.size(); i++) {
        size *= dims[i];
    }
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
