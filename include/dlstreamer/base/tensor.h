/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace dlstreamer {

namespace tensor::key {
static constexpr auto offset = "offset";           // (size_t)
static constexpr auto plane_index = "plane_index"; // int
static constexpr auto offset_x = "offset_x";       // int
static constexpr auto offset_y = "offset_y";       // int
static constexpr auto data = "data";               // void*
}; // namespace tensor::key

class BaseTensor : public Tensor {
  public:
    BaseTensor(MemoryType memory_type, const TensorInfo &info, std::string_view primary_key = {},
               ContextPtr context = nullptr)
        : _memory_type(memory_type), _info(info), _primary_key(primary_key), _context(context) {
    }

    MemoryType memory_type() const override {
        return _memory_type;
    }

    void *data() const override {
        throw std::runtime_error("This tensor object doesn't support access by pointer. "
                                 "Use MemoryMapper objects to map into pointer-backed memory");
    }

    handle_t handle(std::string_view key) const override {
        if (!key.empty()) {
            auto it = _handles.find(key);
            if (it == _handles.end())
                throw std::runtime_error("Handle not found for key = " + std::string(key));
            return it->second;
        } else if (!_primary_key.empty()) {
            auto it = _handles.find(_primary_key);
            if (it == _handles.end())
                throw std::runtime_error("Handle not found for primary key = " + _primary_key);
            return it->second;
        } else {
            return reinterpret_cast<handle_t>(data());
        }
    }

    handle_t handle(std::string_view key, handle_t default_value) const noexcept override {
        auto it = _handles.find(key);
        return (it != _handles.end()) ? it->second : default_value;
    }

    const TensorInfo &info() const override {
        return _info;
    }

    ContextPtr context() const override {
        return _context;
    }

    TensorPtr parent() const override {
        return _parent;
    }

    void set_parent(TensorPtr parent) {
        _parent = parent;
    }

    void set_handle(const std::string &key, handle_t handle) {
        _handles[key] = handle;
    }

  protected:
    MemoryType _memory_type;
    TensorInfo _info;
    std::string _primary_key;
    ContextPtr _context;
    std::map<std::string, handle_t, std::less<void>> _handles; // std::less<void> to support access by std::string_view
    TensorPtr _parent;
};

} // namespace dlstreamer
