/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include <dlstreamer/level_zero/context.h>

// SYCL and oneAPI extension
#include <CL/sycl.hpp>
#include <sycl/ext/oneapi/backend/level_zero.hpp>

namespace dlstreamer {

class SYCLContext;
using SYCLContextPtr = std::shared_ptr<SYCLContext>;

class SYCLContext : public LevelZeroContext {
  public:
    struct key {
        static constexpr auto sycl_queue = "sycl_queue"; // sycl::queue*
    };

    static inline SYCLContextPtr create(sycl::queue sycl_queue) {
        return std::make_shared<SYCLContext>(sycl_queue);
    }

    SYCLContext(sycl::queue sycl_queue)
        : LevelZeroContext(sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue.get_context()),
                           sycl::get_native<sycl::backend::ext_oneapi_level_zero>(sycl_queue.get_device())),
          _sycl_queue(sycl_queue) {
    }

    sycl::queue sycl_queue() const noexcept {
        return _sycl_queue;
    }

    handle_t handle(std::string_view key) const noexcept override {
        if (key == key::sycl_queue)
            return (void *)&_sycl_queue;
        return LevelZeroContext::handle(key);
    }

    std::vector<std::string> keys() const override {
        return {key::sycl_queue};
    }

    inline MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override;

    template <typename T>
    T *malloc(size_t count, sycl::usm::alloc kind) {
        return sycl::malloc<T>(count, _sycl_queue, kind);
    }

    template <typename T>
    void free(T *ptr) {
        // sycl::free sometimes crashes on iGPU, is it DPC++ bug on iGPU?
        sycl::free(ptr, _sycl_queue);
    }

  private:
    sycl::queue _sycl_queue;
};

} // namespace dlstreamer

/////////////////////////////////////////////////////////////////////////////
// Mappers

#include <dlstreamer/sycl/mappers/sycl_usm_to_cpu.h>

namespace dlstreamer {

MemoryMapperPtr SYCLContext::get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) {
    auto mapper = LevelZeroContext::get_mapper(input_context, output_context);
    if (mapper)
        return mapper;

    auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
    auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
    if (input_type == MemoryType::USM && output_type == MemoryType::CPU)
        mapper = std::make_shared<MemoryMapperSYCLUSMToCPU>(input_context, output_context);

    BaseContext::attach_mapper(mapper);
    return mapper;
}

} // namespace dlstreamer
