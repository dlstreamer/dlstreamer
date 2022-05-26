/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/gst/buffer.h"
#include "dlstreamer/openvino/buffer.h"

namespace dlstreamer {

class BufferMapperOpenVINOToCPU : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<OpenVINOBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to OpenVinoBufferBase");
        return map(std::move(buffer), mode);
    }

    CPUBufferPtr map(OpenVinoBufferPtr buffer, AccessMode /*mode*/) {
        // ITT_TASK(__FUNCTION__);
        auto info = buffer->info();

        {
            // ITT_TASK("Wait infer request");
            buffer->wait();
        }

        std::vector<void *> data(info->planes.size());
        for (size_t i = 0; i < info->planes.size(); i++) {
            data[i] = buffer->data(i);
        }

        return std::make_shared<CPUBuffer>(buffer->info(), data);
    }
};

} // namespace dlstreamer
