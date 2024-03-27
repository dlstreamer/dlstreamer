/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/opencl/tensor.h"
#include "dlstreamer/openvino/context.h"
#include "dlstreamer/openvino/tensor.h"

#include <openvino/runtime/intel_gpu/properties.hpp>

namespace dlstreamer {

class MemoryMapperOpenCLToOpenVINO : public BaseMemoryMapper {
  public:
    MemoryMapperOpenCLToOpenVINO(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
        _ov_context = *ptr_cast<OpenVINOContext>(output_context);
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        cl_mem clmem = *ptr_cast<OpenCLTensor>(src);
        auto &info = src->info();

        ov::AnyMap params = {{ov::intel_gpu::shared_mem_type.name(), "OCL_BUFFER"},
                             {ov::intel_gpu::mem_handle.name(), static_cast<void *>(clmem)}};
        auto tensor = _ov_context.create_tensor(data_type_to_openvino(info.dtype), info.shape, params);

        auto ret = std::make_shared<OpenVINOTensor>(tensor, _output_context);
        ret->set_parent(src);
        return ret;
    }

  private:
    ov::RemoteContext _ov_context;
};

} // namespace dlstreamer
