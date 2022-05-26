/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <variant>
#include <vector>

namespace dlstreamer {

using Any = std::variant<int, double, bool, std::string, intptr_t>;

template <typename T>
inline T AnyCast(const Any &any) {
    return std::get<T>(any);
}

template <typename T>
inline bool AnyHoldsType(const Any &any) {
    return std::holds_alternative<T>(any);
}

class Dictionary {
  public:
    virtual ~Dictionary(){};

    virtual std::string name() const = 0;

    virtual std::optional<Any> try_get(std::string const &key) const noexcept = 0;

    virtual void set(std::string const &key, Any value) = 0;

    virtual std::vector<std::string> keys() const = 0;

    template <typename T>
    inline T get(std::string const &key) const {
        std::optional<Any> opt_val = try_get(key);
        if (opt_val)
            return AnyCast<T>(*opt_val);
        throw std::out_of_range("Dictionary key not found: " + key);
    }

    template <typename T>
    inline T get(std::string const &key, T default_value) const {
        std::optional<Any> opt_val = try_get(key);
        if (opt_val)
            return AnyCast<T>(*opt_val);
        return default_value;
    }
};

using DictionaryPtr = std::shared_ptr<Dictionary>;

using DictionaryCPtr = std::shared_ptr<const Dictionary>;

using DictionaryVector = std::vector<DictionaryPtr>;

class STDDictionary : public Dictionary {
  public:
    STDDictionary() = default;
    STDDictionary(const std::string &name) : _name(name) {
    }
    STDDictionary(const std::string &name, const std::map<std::string, Any> &map) : _name(name), _map(map) {
    }

    std::string name() const override {
        return _name;
    }

    std::optional<Any> try_get(std::string const &key) const noexcept override {
        auto it = _map.find(key);
        if (it == _map.end())
            return {};
        return it->second;
    }

    void set(std::string const &key, Any value) override {
        _map[key] = value;
    }

    std::vector<std::string> keys() const override {
        std::vector<std::string> ret;
        for (auto it : _map) {
            ret.push_back(it.first);
        }
        return ret;
    }

    inline bool operator<(const STDDictionary &r) const {
        const STDDictionary &l = *this;
        return std::tie(l._name, l._map) < std::tie(r._name, r._map);
    }

  protected:
    std::string _name;
    std::map<std::string, Any> _map;
};

class DictionaryProxy : public Dictionary {
  public:
    DictionaryProxy(DictionaryPtr dict) {
        if (!dict)
            throw std::runtime_error("DictionaryProxy created on nullptr");
        _dict = dict;
    }

    std::string name() const override {
        return _dict->name();
    }

    std::optional<Any> try_get(std::string const &key) const noexcept override {
        return _dict->try_get(key);
    }

    void set(std::string const &key, Any value) override {
        return _dict->set(key, value);
    }

    std::vector<std::string> keys() const override {
        return _dict->keys();
    }

  protected:
    DictionaryPtr _dict;
};

} // namespace dlstreamer
