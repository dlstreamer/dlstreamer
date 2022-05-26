/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "dlstreamer/buffer.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#pragma once

namespace dlstreamer {

static inline DictionaryPtr find_metadata(const Buffer &buffer, std::string_view meta_name) {
    for (auto &meta : buffer.metadata()) {
        if (meta->name() == meta_name)
            return meta;
    }
    return nullptr;
}

template <class T>
static inline std::shared_ptr<T> find_metadata(const Buffer &buffer) {
    auto meta = find_metadata(buffer, T::name);
    return meta ? std::make_shared<T>(meta) : nullptr;
}

static inline void copy_dictionary(const Dictionary &src, Dictionary &dst) {
    for (auto &key : src.keys()) {
        dst.set(key, *src.try_get(key));
    }
}

static inline void copy_metadata(const Buffer &src, Buffer &dst) {
    for (auto &src_meta : src.metadata()) {
        auto dst_meta = dst.add_metadata(src_meta->name());
        copy_dictionary(*src_meta, *dst_meta);
    }
}

static inline std::string any_to_string(Any value) {
    if (AnyHoldsType<int>(value)) {
        return std::to_string(AnyCast<int>(value));
    } else if (AnyHoldsType<double>(value)) {
        return std::to_string(AnyCast<double>(value));
    } else if (AnyHoldsType<bool>(value)) {
        return std::to_string(AnyCast<bool>(value));
    } else if (AnyHoldsType<std::string>(value)) {
        return AnyCast<std::string>(value);
    } else if (AnyHoldsType<intptr_t>(value)) {
        return std::to_string(AnyCast<intptr_t>(value));
    } else {
        throw std::runtime_error("Unsupported data type");
    }
}

static inline std::string datatype_to_string(DataType datatype) {
    switch (datatype) {
    case DataType::U8:
        return "uint8";
    case DataType::FP32:
        return "float32";
    case DataType::I32:
        return "int32";
    default:
        throw std::runtime_error("Unknown DataType");
    }
}

static inline DataType datatype_from_string(const std::string &str) {
    if (str == "uint8")
        return DataType::U8;
    else if (str == "float32")
        return DataType::FP32;
    else if (str == "int32")
        return DataType::I32;
    else
        throw std::runtime_error("Unknown DataType string " + str);
}

static inline std::vector<std::string> split_string(const std::string &input, char delimiter = ',') {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

template <typename Iter>
static inline std::string join_strings(Iter begin, Iter end, char delimiter = ',') {
    std::ostringstream result;
    for (auto iter = begin; iter != end; iter++) {
        if (iter == begin)
            result << *iter;
        else
            result << delimiter << *iter;
    }
    return result.str();
}

static inline std::vector<float> string_to_float_array(const std::string input, char delimiter = ',') {
    std::vector<float> output;
    std::istringstream tokenStream(input);
    std::string token;
    while (std::getline(tokenStream, token, delimiter)) {
        output.push_back(std::stof(token));
    }
    return output;
}

static inline std::vector<size_t> shape_from_string(const std::string &str) {
    auto tokens = split_string(str, ':');
    std::vector<size_t> result;
    result.reserve(tokens.size());
    for (const auto &token : tokens)
        result.emplace_back(std::stoul(token));
    std::reverse(result.begin(), result.end()); // reverse order
    return result;
}

static inline std::string shape_to_string(std::vector<size_t> dims) {
    std::reverse(dims.begin(), dims.end()); // reverse order
    return join_strings(dims.begin(), dims.end(), ':');
}

} // namespace dlstreamer
