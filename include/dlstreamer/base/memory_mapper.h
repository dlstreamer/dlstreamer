/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/memory_mapper.h"

namespace dlstreamer {

class BaseMemoryMapper : public MemoryMapper {
  public:
    using MemoryMapper::map;

    BaseMemoryMapper(const ContextPtr &input_context, const ContextPtr &output_context)
        : _input_context(input_context), _output_context(output_context) {
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        return src;
    }

    FramePtr map(FramePtr src, AccessMode mode) override {
        MemoryType memory_type = _output_context ? _output_context->memory_type() : MemoryType::CPU;
        auto dst = std::make_shared<BaseFrame>(src->media_type(), src->format(), memory_type);
        for (auto &tensor : src) {
            dst->_tensors.push_back(map(tensor, mode));
        }
        dst->set_parent(src);
        return dst;
    }

    ContextPtr input_context() const override {
        return _input_context;
    }

    ContextPtr output_context() const override {
        return _output_context;
    }

  protected:
    ContextPtr _input_context;
    ContextPtr _output_context;
};

} // namespace dlstreamer
