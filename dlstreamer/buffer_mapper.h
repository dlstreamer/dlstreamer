/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer.h"

#include <memory>

namespace dlstreamer {

enum class AccessMode { READ = 1, WRITE = 2, READ_WRITE = 3 };

class BufferMapper {
  public:
    virtual ~BufferMapper() = default;

    virtual BufferPtr map(BufferPtr src_buffer, AccessMode mode) = 0;

    template <typename T>
    inline std::shared_ptr<T> map(BufferPtr src_buffer, AccessMode mode) {
        BufferPtr dst_buffer = map(std::move(src_buffer), mode);
        auto dst = std::dynamic_pointer_cast<T>(dst_buffer);
        if (!dst)
            throw std::runtime_error("Failed casting BufferPtr");
        return dst;
    }
};

using BufferMapperPtr = std::shared_ptr<BufferMapper>;

} // namespace dlstreamer
