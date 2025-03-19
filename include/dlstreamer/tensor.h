/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/context.h"
#include "dlstreamer/memory_mapper.h"
#include "dlstreamer/memory_type.h"
#include "dlstreamer/tensor_info.h"

#include <assert.h>
#include <cstdint>
#include <memory>

namespace dlstreamer {

/**
 * @brief Tensor is a multidimensional array. Tensors are similar to NumPy arrays, with main difference that tensor
 * may allocate memory on GPU. Classes inherited from Tensors implement interface function via underlying frameworks (
 * for example OpenCL, DPC++, OpenCV, etc) and provide access to framework specific memory objects (for example, cl_mem,
 * USM pointers, cv::Mat, etc).
 */
class Tensor {
  public:
    using handle_t = intptr_t;

    Tensor() = default;
    Tensor(const Tensor &) = delete;
    Tensor &operator=(const Tensor &) = delete;
    virtual ~Tensor() {
    }

    /**
     * @brief Returns tensor information - data type, shape, stride.
     * @param key the key of the handle to find
     */
    virtual const TensorInfo &info() const = 0;

    /**
     * @brief Returns tensor's memory type.
     */
    virtual MemoryType memory_type() const = 0;

    /**
     * @brief Returns context used to create tensor. The context()->memory_type() returns same type as memory_type().
     * Function may return nullptr if tensor created without context, for example CPU-memory tensor.
     */
    virtual ContextPtr context() const = 0;

    /**
     * @brief Returns pointer to tensor data. If underlying memory allocation relies on abstract memory handle (for
     * example, cl_mem), this function returns nullptr.
     */
    virtual void *data() const = 0;

    /**
     * @brief Returns a handle by key. If empty key, returns default handle. If no handle with the specified key,
     * exception is thrown.
     * @param key the key of the handle to find
     */
    virtual handle_t handle(std::string_view key = {}) const = 0;

    /**
     * @brief Returns a handle by key. If empty key, returns default handle. If no handle with the specified key,
     * return default value specified in function parameter.
     * @param key the key of the handle to find
     * @param default_value default value
     */
    virtual handle_t handle(std::string_view key, handle_t default_value) const noexcept = 0;

    /**
     * @brief Returns parent tensor if this tensor was mapped (using MemoryMapper) from another tensor or contains
     * sub-region of another tensor, otherwise returns nullptr.
     */
    virtual TensorPtr parent() const = 0;

    template <typename T>
    T *data() const {
        if (!check_datatype<T>(info().dtype))
            throw std::runtime_error("Accessing tensor with incompatible data type");
        return reinterpret_cast<T *>(data());
    }

    template <typename T>
    T *data(std::vector<size_t> offset, bool left_offset = true) const {
        const auto &tinfo = info();
        if (!check_datatype<T>(tinfo.dtype))
            throw std::runtime_error("Accessing tensor with incompatible data type");
        const auto &stride = tinfo.stride;
        size_t byte_offset = 0;
        if (left_offset) {
            for (size_t i = 0; i < offset.size(); i++)
                byte_offset += offset[i] * stride[i];
        } else {
            for (size_t i = 0; i < offset.size(); i++)
                byte_offset += offset[offset.size() - 1 - i] * stride[stride.size() - 1 - i];
        }
        return reinterpret_cast<T *>(static_cast<uint8_t *>(data()) + byte_offset);
    }
};

class TensorPtr : public std::shared_ptr<Tensor> {
  public:
    using std::shared_ptr<Tensor>::shared_ptr; // inherit constructors

    TensorPtr map(const ContextPtr &output_context = nullptr, AccessMode access_mode = AccessMode::ReadWrite) {
        auto input_context = get()->context();
        if (input_context == output_context)
            return *this;
        MemoryMapperPtr mapper = nullptr;
        if (output_context)
            mapper = output_context->get_mapper(input_context, output_context);
        if (!mapper && input_context)
            mapper = input_context->get_mapper(input_context, output_context);
        if (!mapper) {
            auto to_string = [](ContextPtr context) {
                MemoryType memory_type = context ? context->memory_type() : MemoryType::CPU;
                return std::string(memory_type_to_string(memory_type));
            };
            throw std::runtime_error("Error getting mapper from " + to_string(input_context) + " to " +
                                     to_string(output_context));
        }
        return mapper->map(*this, access_mode);
    }

    TensorPtr map(AccessMode access_mode = AccessMode::ReadWrite) {
        return map(nullptr, access_mode);
    }

    template <typename T>
    inline std::shared_ptr<T> map(const ContextPtr &output_context, AccessMode access_mode = AccessMode::ReadWrite) {
        return ptr_cast<T>(map(output_context, access_mode));
    }
};

using TensorVector = std::vector<TensorPtr>;

} // namespace dlstreamer
