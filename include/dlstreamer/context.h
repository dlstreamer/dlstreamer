/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/memory_type.h"
#include <string>

namespace dlstreamer {

class Context;
using ContextPtr = std::shared_ptr<Context>;
class MemoryMapper;
using MemoryMapperPtr = std::shared_ptr<MemoryMapper>;

/**
 * @brief This class represents context created on one of memory types listed in enum MemoryType. Context contains one
 * or multiple handles associated with underlying framework or memory type, for example handle with type cl_context and
 * key "cl_context" if OpenCL memory type. Context is capable to create memory mapping object for mapping to or from
 * system memory, and potentially other memory types if supported by underlying framework. Context is capable to create
 * another context on same device, if supported by underlying framework.
 */
class Context {
  public:
    /**
     * @brief Type of handles stored in context.
     */
    using handle_t = void *;

    virtual ~Context(){};

    /**
     * @brief Returns memory type of this context.
     */
    virtual MemoryType memory_type() const = 0;

    /**
     * @brief Returns a handle by key. If empty key, returns default handle. If no handle with the specified key,
     * returns 0.
     * @param key the key of the handle to find
     */
    virtual handle_t handle(std::string_view key = {}) const noexcept = 0;

    /**
     * @brief Returns an object for memory mapping between two contexts. Function typically expects one of contexts to
     * be this context. If one of contexts has memory type CPU or context reference is nullptr, function returns object
     * for mapping to or from system memory. If creating memory mapping object failed, exception is thrown.
     * @param input_context Context of input memory
     * @param output_context Context of output memory
     */
    virtual MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) = 0;

    /**
     * @brief Create another context of specified memory type. New context belongs to same device (if multiple GPUs) as
     * original context, and parent() returns reference to original context. If creating new context failed, function
     * returns nullptr.
     * @param memory_type Memory type of new context
     */
    virtual ContextPtr derive_context(MemoryType memory_type) noexcept = 0;

    /**
     * @brief Returns parent context if this context was created from another context via derive_context(), nullptr
     * otherwise.
     */
    virtual ContextPtr parent() noexcept = 0;
};

} // namespace dlstreamer
