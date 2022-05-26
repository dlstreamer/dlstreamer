/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/buffer_mapper.h>
#include <list>
#include <numeric>

namespace dlstreamer {

class BufferMapperChain final : public BufferMapper {
    std::vector<BufferMapperPtr> _chain;

  public:
    BufferMapperChain(std::initializer_list<BufferMapperPtr> l) : _chain(l) {
    }
    BufferMapperChain(const std::vector<BufferMapperPtr> &v) : _chain(v) {
    }

    dlstreamer::BufferPtr map(dlstreamer::BufferPtr src_buffer, dlstreamer::AccessMode mode) override {
        return std::accumulate(
            _chain.begin(), _chain.end(), std::move(src_buffer),
            [mode](BufferPtr buf, BufferMapperPtr mapper) { return mapper->map(std::move(buf), mode); });
    }
};

} // namespace dlstreamer
