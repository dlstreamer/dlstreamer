/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dictionary.h"
#include "dlstreamer/frame_info.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#pragma once

#define DLS_CHECK(_VAR)                                                                                                \
    if (!(_VAR))                                                                                                       \
        throw std::runtime_error(std::string(__FUNCTION__) + ": Error on: " #_VAR);

#define DLS_CHECK_GE0(_VAR)                                                                                            \
    {                                                                                                                  \
        auto _res = _VAR;                                                                                              \
        if (_res < 0)                                                                                                  \
            throw std::runtime_error(std::string(__FUNCTION__) + ": Error " + std::to_string(_res) +                   \
                                     " calling: " #_VAR);                                                              \
    }

namespace dlstreamer {

template <typename T>
using auto_ptr = std::unique_ptr<T, std::function<void(T *)>>;

static inline FrameInfo frame_info(FramePtr frame) {
    FrameInfo info;
    MemoryType memory_type = MemoryType::Any;
    for (size_t i = 0; i < frame->num_tensors(); i++) {
        auto tensor = frame->tensor(i);
        info.tensors.push_back(tensor->info());
        if (memory_type == MemoryType::Any)
            memory_type = tensor->memory_type();
        else if (memory_type != tensor->memory_type())
            throw std::runtime_error("Inconsistent memory type");
    }
    info.memory_type = memory_type;
    info.media_type = frame->media_type();
    info.format = frame->format();
    return info;
}

static inline std::vector<std::string> split_string(const std::string &input, char delimiter = ',') {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(input);
    while (std::getline(token_stream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

template <typename OUTPUT, typename INPUT, typename O>
static inline std::vector<OUTPUT> transform_vector(const std::vector<INPUT> &input, O op) {
    std::vector<OUTPUT> output;
    std::transform(input.begin(), input.end(), std::back_inserter(output), op);
    return output;
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

static inline DictionaryPtr find_metadata(const Frame &frame, std::string_view meta_name) {
    for (auto &meta : frame.metadata()) {
        if (meta->name() == meta_name)
            return meta;
    }
    return nullptr;
}

template <class T>
static inline std::shared_ptr<T> find_metadata(const Frame &frame, std::string_view meta_name = T::name) {
    auto meta = find_metadata(frame, meta_name);
    return meta ? std::make_shared<T>(meta) : nullptr;
}

template <class T>
static inline std::shared_ptr<T> find_metadata(const Frame &frame, std::string_view meta_name,
                                               std::string_view format) {
    // try to find by meta_name
    auto mask_meta = find_metadata<T>(frame, meta_name);
    if (mask_meta && mask_meta->format() == format)
        return mask_meta;
    // try to find by format
    for (auto &meta : frame.metadata()) {
        if (T(meta).format() == format)
            return std::make_shared<T>(meta);
    }
    return nullptr;
}

template <class T>
static inline T add_metadata(Frame &frame, std::string_view meta_name = T::name) {
    return T(frame.metadata().add(meta_name));
}

static inline void copy_dictionary(const Dictionary &src, Dictionary &dst, bool copy_name = false) {
    for (auto &key : src.keys()) {
        dst.set(key, *src.try_get(key));
    }
    if (copy_name) {
        dst.set_name(src.name());
    }
}

static inline void copy_metadata(const Frame &src, Frame &dst) {
    for (auto &src_meta : src.metadata()) {
        auto dst_meta = dst.metadata().add(src_meta->name());
        copy_dictionary(*src_meta, *dst_meta);
    }
}

static inline std::string any_to_string(Any value) {
    if (any_holds_type<int>(value)) {
        return std::to_string(any_cast<int>(value));
    } else if (any_holds_type<double>(value)) {
        return std::to_string(any_cast<double>(value));
    } else if (any_holds_type<bool>(value)) {
        return std::to_string(any_cast<bool>(value));
    } else if (any_holds_type<std::string>(value)) {
        return any_cast<std::string>(value);
    } else if (any_holds_type<intptr_t>(value)) {
        return std::to_string(any_cast<intptr_t>(value));
    } else if (any_holds_type<std::vector<double>>(value)) {
        auto vec = any_cast<std::vector<double>>(value);
        return join_strings(vec.cbegin(), vec.cend());
    } else {
        throw std::runtime_error("Unsupported data type");
    }
}

static inline std::string datatype_to_string(DataType datatype) {
    switch (datatype) {
    case DataType::UInt8:
        return "uint8";
    case DataType::Float32:
        return "float32";
    case DataType::Int32:
        return "int32";
    case DataType::Int64:
        return "int64";
    default:
        throw std::runtime_error("Unknown DataType");
    }
}

static inline DataType datatype_from_string(const std::string &str) {
    if (str == "uint8")
        return DataType::UInt8;
    else if (str == "float32")
        return DataType::Float32;
    else if (str == "int32")
        return DataType::Int32;
    else if (str == "int64")
        return DataType::Int64;
    else
        throw std::runtime_error("Unknown DataType string " + str);
}

static inline std::string media_type_to_string(MediaType media_type) {
    switch (media_type) {
    case MediaType::Any:
        return "Any";
    case MediaType::Tensors:
        return "Tensors";
    case MediaType::Image:
        return "Image";
    case MediaType::Audio:
        return "Audio";
    }
    return "Unknown";
}

static inline std::vector<size_t> shape_from_string(const std::string &str) {
    auto stoul = [](const std::string &str) { return std::stoul(str); };
    return transform_vector<size_t>(split_string(str, ':'), stoul);
}

static inline std::string shape_to_string(const std::vector<size_t> &dims) {
    return join_strings(dims.cbegin(), dims.cend(), ':');
}

static inline std::string tensor_info_to_string(const TensorInfo &info) {
    auto str = datatype_to_string(info.dtype) + ", " + shape_to_string(info.shape);
    if (!info.is_contiguous())
        str += ", " + shape_to_string(info.stride);
    return str;
}

static inline TensorInfo tensor_info_from_string(const std::string &str) {
    auto strings = split_string(str);
    TensorInfo info;
    info.dtype = datatype_from_string(strings[0]);
    info.shape = shape_from_string(strings[1]);
    if (strings.size() > 2)
        info.stride = shape_from_string(strings[2]);
    return info;
}

static inline std::string frame_info_to_string(const FrameInfo &finfo) {
    std::string str = media_type_to_string(finfo.media_type) + ", " + memory_type_to_string(finfo.memory_type);
    if (finfo.media_type == MediaType::Image)
        str += ", " + image_format_to_string(static_cast<ImageFormat>(finfo.format));
    for (auto &tinfo : finfo.tensors) {
        str += ", {";
        str += tensor_info_to_string(tinfo);
        str += "}";
    }
    return str;
}

} // namespace dlstreamer
