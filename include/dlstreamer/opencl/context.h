/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include "dlstreamer/opencl/mappers/dma_to_opencl.h"
#include "dlstreamer/opencl/mappers/opencl_to_cpu.h"
#include "dlstreamer/opencl/mappers/opencl_to_dma.h"

#include <CL/cl.h>

namespace dlstreamer {

class OpenCLContext;
using OpenCLContextPtr = std::shared_ptr<OpenCLContext>;

class OpenCLContext : public BaseContext {
  public:
    struct key {
        static constexpr auto cl_context = BaseContext::key::cl_context;
        static constexpr auto cl_queue = BaseContext::key::cl_queue;
    };

    static inline OpenCLContextPtr create(const ContextPtr &another_context) {
        return create_from_another<OpenCLContext>(another_context, MemoryType::OpenCL);
    }

    OpenCLContext(cl_context ctx, cl_command_queue queue = nullptr)
        : BaseContext(MemoryType::OpenCL), _ctx(ctx), _queue(queue) {
        DLS_CHECK(_ctx);
        clRetainContext(_ctx);
        if (_queue)
            clRetainCommandQueue(_queue);
    }

    OpenCLContext(const ContextPtr &another_context) : BaseContext(MemoryType::OpenCL) {
        DLS_CHECK(another_context);
        DLS_CHECK(_ctx = static_cast<cl_context>(another_context->handle(key::cl_context)));
        clRetainContext(_ctx);
        _queue = static_cast<cl_command_queue>(another_context->handle(key::cl_queue));
        if (_queue)
            clRetainCommandQueue(_queue);
        _parent = another_context;
    }

    ~OpenCLContext() {
        if (_ctx)
            clReleaseContext(_ctx);
        if (_queue)
            clReleaseCommandQueue(_queue);
    }

    OpenCLContext(const OpenCLContext &) = delete;
    OpenCLContext &operator=(const OpenCLContext &) = delete;
    OpenCLContext(OpenCLContext &&) = delete;
    OpenCLContext &operator=(OpenCLContext &&) = delete;

    cl_context context() {
        return _ctx;
    }

    cl_command_queue queue() {
        return _queue;
    }

    std::vector<std::string> keys() const override {
        return {key::cl_context};
    }

    handle_t handle(std::string_view key) const noexcept override {
        if (key == key::cl_context || key.empty())
            return _ctx;
        return nullptr;
    }

    void flush() {
        DLS_CHECK(_queue)
        clFlush(_queue);
    }

    void finish() {
        DLS_CHECK(_queue)
        clFinish(_queue);
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::DMA && output_type == MemoryType::OpenCL)
            mapper = std::make_shared<MemoryMapperDMAToOpenCL>(input_context, output_context);
        if (input_type == MemoryType::OpenCL && output_type == MemoryType::CPU)
            return std::make_shared<MemoryMapperOpenCLToCPU>(input_context, output_context);
        if (input_type == MemoryType::OpenCL && output_type == MemoryType::DMA)
            return std::make_shared<MemoryMapperOpenCLToDMA>(input_context, output_context);

        BaseContext::attach_mapper(mapper);
        return mapper;
    }

  protected:
    cl_context _ctx;
    cl_command_queue _queue;
};

} // namespace dlstreamer
