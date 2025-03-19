/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dictionary.h"
#include <map>

namespace dlstreamer {

class BaseDictionary : public Dictionary {
  public:
    BaseDictionary() = default;
    BaseDictionary(std::string_view name) : _name(name) {
    }
    BaseDictionary(const AnyMap &map) : _map(map) {
    }
    BaseDictionary(const std::string &name, const AnyMap &map) : _name(name), _map(map) {
    }

    std::string name() const override {
        return _name;
    }

    std::optional<Any> try_get(std::string_view key) const noexcept override {
        auto it = _map.find(key);
        if (it == _map.end())
            return {};
        return it->second;
    }

    std::pair<const void *, size_t> try_get_array(std::string_view key) const noexcept override {
        auto it = _arrays.find(key);
        if (it == _arrays.end())
            return {nullptr, 0};
        return {it->second.data(), it->second.size()};
    }

    void set(std::string_view key, Any value) override {
        _map[std::string(key)] = value;
    }

    void set_array(std::string_view key, const void *data, size_t nbytes) override {
        const uint8_t *data_u8 = static_cast<const uint8_t *>(data);
        _arrays[std::string(key)] = std::vector<uint8_t>(data_u8, data_u8 + nbytes);
    }

    void set_name(std::string const &name) override {
        _name = name;
    }

    std::vector<std::string> keys() const override {
        std::vector<std::string> ret;
        for (auto it : _map) {
            ret.push_back(it.first);
        }
        return ret;
    }

    inline bool operator<(const BaseDictionary &r) const {
        const BaseDictionary &l = *this;
        return std::tie(l._name, l._map) < std::tie(r._name, r._map);
    }

  protected:
    std::string _name;
    AnyMap _map;
    std::map<std::string, std::vector<uint8_t>, std::less<void>> _arrays; // std::less to support access by string_view
};

} // namespace dlstreamer
