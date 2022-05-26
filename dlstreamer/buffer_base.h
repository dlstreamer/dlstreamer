/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer.h"
#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace dlstreamer {

class BufferBase : public Buffer {
  protected:
    BufferBase(BufferType type, BufferInfoCPtr info, ContextPtr context = nullptr)
        : _type(type), _info(info), _context(context) {
    }

  public:
    BufferType type() const override {
        return _type;
    }
    void *data(size_t /*plane_index = 0*/) const override {
        return nullptr;
    }
    std::vector<std::string> keys() const override {
        std::set<std::string> keys;
        for (auto &handle : _handles) {
            auto key = handle.first;
            auto pos = key.find('%');
            keys.insert(pos != std::string::npos ? key.substr(0, pos) : key);
        }
        return {keys.begin(), keys.end()};
    }
    handle_t handle(std::string const &handle_id, size_t plane_index = 0) const override {
        return _handles.at(full_id(handle_id, plane_index));
    }
    handle_t handle(std::string const &handle_id, size_t plane_index, size_t default_value) const noexcept override {
        auto it = _handles.find(full_id(handle_id, plane_index));
        return (it != _handles.end()) ? it->second : default_value;
    }
    BufferInfoCPtr info() const override {
        return _info;
    }
    ContextPtr context() const override {
        return _context;
    }
    DictionaryVector metadata() const override {
        return _metadata;
    }
    DictionaryPtr add_metadata(const std::string &name) override {
        DictionaryPtr meta = std::make_shared<STDDictionary>(name);
        _metadata.push_back(meta);
        return meta;
    }
    void remove_metadata(DictionaryPtr meta) override {
        auto it = std::find(_metadata.cbegin(), _metadata.cend(), meta);
        if (it != _metadata.cend())
            _metadata.erase(it);
    }

    void add_handle(std::string handle_id, size_t plane_index, handle_t handle) override {
        _handles[full_id(handle_id, plane_index)] = static_cast<handle_t>(handle);
    }

  protected:
    BufferType _type;
    BufferInfoCPtr _info;
    ContextPtr _context;
    std::map<std::string, handle_t> _handles;
    DictionaryVector _metadata;

    static std::string full_id(std::string const &handle_id, size_t plane_index) {
        return handle_id + "%" + std::to_string(plane_index);
    }

    template <typename T>
    void set_handle(std::string handle_id, size_t plane_index, T handle) {
        add_handle(handle_id, plane_index, static_cast<handle_t>(handle));
    }
};

class CPUBuffer : public BufferBase {
  public:
    CPUBuffer(BufferInfoCPtr info, const std::vector<void *> &data) : BufferBase(BufferType::CPU, info), _data(data) {
    }

    void *data(size_t plane_index = 0) const override {
        assert(plane_index < _data.size());
        return _data[plane_index];
    }

  private:
    std::vector<void *> _data;
};

using CPUBufferPtr = std::shared_ptr<CPUBuffer>;

} // namespace dlstreamer
