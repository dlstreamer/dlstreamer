/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include "dlstreamer/opencv_umat/mappers/opencl_to_opencv_umat.h"
#include "dlstreamer/opencv_umat/utils.h"

namespace dlstreamer {

class OpenCVUMatContext;
using OpenCVUMatContextPtr = std::shared_ptr<OpenCVUMatContext>;

class OpenCVUMatContext : public BaseContext {
  public:
    static inline OpenCVUMatContextPtr create(ContextPtr /*another_context*/) {
        return std::make_shared<OpenCVUMatContext>();
    }

    OpenCVUMatContext() : BaseContext(MemoryType::OpenCVUMat) {
        cv::ocl::setUseOpenCL(true);
    }

    handle_t handle(std::string_view key) const noexcept override {
        if (key == BaseContext::key::cl_context) {
            auto &ocl_context = cv::ocl::OpenCLExecutionContext::getCurrent();
            if (!ocl_context.empty())
                return ocl_context.getContext().ptr();
        }
        if (key == BaseContext::key::cl_queue) {
            auto &ocl_context = cv::ocl::OpenCLExecutionContext::getCurrent();
            if (!ocl_context.empty())
                return ocl_context.getQueue().ptr();
        }
        return nullptr;
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::OpenCL && output_type == MemoryType::OpenCVUMat)
            return std::make_shared<MemoryMapperOpenCLToOpenCVUMat>(input_context, output_context);

        BaseContext::attach_mapper(mapper);
        return mapper;
    }

    void finish() {
        auto &ocl_context = cv::ocl::OpenCLExecutionContext::getCurrent();
        if (!ocl_context.empty())
            ocl_context.getQueue().finish();
    }
};

} // namespace dlstreamer
