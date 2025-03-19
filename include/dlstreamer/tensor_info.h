/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace dlstreamer {

enum class DataType { UInt8 = 1, Int32 = 2, Int64 = 3, Float32 = 4 };

static inline size_t datatype_size(DataType datatype);
static inline std::vector<size_t> contiguous_stride(const std::vector<size_t> &shape, DataType type);

/**
 * @brief Structure contaning tensor information - data type, shape, stride.
 */
struct TensorInfo {
    std::vector<size_t> shape;
    std::vector<size_t> stride;
    DataType dtype;

    TensorInfo() = default;

    /**
     * @brief If stride array is empty in constructor parameter, stride will be created automatically assuming
     * contiguous memory layout without padding.
     */
    TensorInfo(std::vector<size_t> _shape, DataType _dtype = DataType::UInt8, std::vector<size_t> _stride = {})
        : shape(std::move(_shape)), stride(std::move(_stride)), dtype(_dtype) {
        if (stride.empty()) {
            stride = contiguous_stride(shape, dtype);
        }
    }

    /**
     * @brief Return number elements in tensor - multiplication of all dimensions in tensor shape.
     */
    size_t size() const {
        return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<size_t>());
    }

    /**
     * @brief Returns number bytes consumed by one element.
     */
    inline size_t itemsize() const {
        return datatype_size(dtype);
    }

    /**
     * @brief Return number bytes required to store tensor data in memory buffer.
     */
    size_t nbytes() const {
        for (size_t i = 0; i < shape.size(); i++) {
            if (shape[i] != 1)
                return stride[i] * shape[i];
        }
        return shape.empty() ? 0 : datatype_size(dtype);
    }

    /**
     * @brief Return true if strides set to contiguous memory layout without padding.
     */
    bool is_contiguous() const {
        auto cstride = contiguous_stride(shape, dtype);
        return std::equal(stride.begin(), stride.end(), cstride.begin());
    }

    inline bool operator<(const TensorInfo &r) const {
        const TensorInfo &l = *this;
        return std::tie(l.shape, l.stride, l.dtype) < std::tie(r.shape, r.stride, r.dtype);
    }

    inline bool operator==(const TensorInfo &r) const {
        const TensorInfo &l = *this;
        return std::tie(l.shape, l.stride, l.dtype) == std::tie(r.shape, r.stride, r.dtype);
    }

    inline bool operator!=(const TensorInfo &r) const {
        return !operator==(r);
    }
};

using TensorInfoVector = std::vector<TensorInfo>;

static size_t datatype_size(DataType datatype) {
    switch (datatype) {
    case DataType::UInt8:
        return 1;
    case DataType::Float32:
        return 4;
    case DataType::Int32:
        return 4;
    case DataType::Int64:
        return 8;
    }
    throw std::runtime_error("Unknown DataType");
};

static std::vector<size_t> contiguous_stride(const std::vector<size_t> &shape, DataType type) {
    std::vector<size_t> stride(shape.size());
    size_t size = datatype_size(type);
    for (int i = shape.size() - 1; i >= 0; i--) {
        stride[i] = size;
        size *= shape[i];
    }
    return stride;
}

template <typename T>
inline bool check_datatype(DataType /*dtype*/) {
    return false;
}
template <>
inline bool check_datatype<uint8_t>(DataType dtype) {
    return (dtype == DataType::UInt8);
}
template <>
inline bool check_datatype<int32_t>(DataType dtype) {
    return (dtype == DataType::Int32);
}
template <>
inline bool check_datatype<int64_t>(DataType dtype) {
    return (dtype == DataType::Int64);
}
template <>
inline bool check_datatype<float>(DataType dtype) {
    return (dtype == DataType::Float32);
}

} // namespace dlstreamer
