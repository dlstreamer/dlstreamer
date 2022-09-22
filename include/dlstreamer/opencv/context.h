/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include "dlstreamer/opencv/mappers/cpu_to_opencv.h"

namespace dlstreamer {

class OpenCVContext : public BaseContext {
  public:
    OpenCVContext() : BaseContext(MemoryType::OpenCV) {
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::CPU && output_type == MemoryType::OpenCV)
            mapper = std::make_shared<MemoryMapperCPUToOpenCV>(input_context, output_context);

        BaseContext::attach_mapper(mapper);
        return mapper;
    }
};

using OpenCVContextPtr = std::shared_ptr<OpenCVContext>;

} // namespace dlstreamer
