/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/opencl/buffer.h"
#include "dlstreamer/openvino/buffer.h"
#include "gpu/gpu_params.hpp"

namespace dlstreamer {

class BufferMapperOpenCLToOpenVINO : public BufferMapper {
  public:
    BufferMapperOpenCLToOpenVINO(IE::RemoteContext::Ptr ie_ctx) : _ie_ctx(ie_ctx) {
    }

    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<OpenCLBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to OpenCLBuffer");
        return map(buffer, mode);
    }

    OpenVINOBlobsBufferPtr map(OpenCLBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        OpenVINOBlobsBuffer::Blobs blobs;

        for (size_t i = 0; i < info->planes.size(); i++) {
            auto desc = plane_info_to_tensor_desc(info->planes[i]);
            // blobs.push_back(IE::gpu::make_shared_blob(desc, _ie_ctx, buffer->clmem(i)));
            cl_mem clmem = buffer->clmem(i);
            IE::ParamMap params = {{IE::GPUContextParams::PARAM_SHARED_MEM_TYPE, IE::GPUContextParams::OCL_BUFFER},
                                   {IE::GPUContextParams::PARAM_MEM_HANDLE, static_cast<IE::gpu_handle_param>(clmem)}};
            auto blob = _ie_ctx->CreateBlob(desc, params);
            blobs.push_back(blob);
        }
        return OpenVINOBlobsBufferPtr(new OpenVINOBlobsBuffer(blobs),
                                      [buffer](OpenVINOBlobsBuffer *dst) { delete dst; });
    }

  private:
    IE::RemoteContext::Ptr _ie_ctx;
};

} // namespace dlstreamer
