/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/buffer_base.h>

namespace dlstreamer {

class UsmBuffer : public BufferBase {
  public:
    UsmBuffer(BufferInfoCPtr info, const std::vector<void *> &data) : BufferBase(BufferType::USM, info), _data(data) {
    }

    UsmBuffer(BufferInfoCPtr info, void *usm_ptr) : BufferBase(BufferType::USM, info) {
        _data.reserve(info->planes.size());
        for (auto &plane : info->planes) {
            _data.emplace_back(static_cast<uint8_t *>(usm_ptr) + plane.offset);
        }
    }

    void *data(size_t plane_index = 0) const override {
        assert(plane_index < _data.size());
        return _data[plane_index];
    }

  private:
    std::vector<void *> _data;
};

using UsmBufferPtr = std::shared_ptr<UsmBuffer>;

} // namespace dlstreamer
