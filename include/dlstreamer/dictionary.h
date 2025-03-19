/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <typeinfo>
#include <variant>
#include <vector>

#include <iostream>

namespace dlstreamer {

/**
 * @brief Type-safe union of selective basic types (defined via std::variant, requires C++17).
 */
using Any = std::variant<
    // basic types
    int, double, size_t, std::string,
    // vector of basic types
    std::vector<int>, std::vector<double>, std::vector<size_t>, std::vector<std::string>,
    // other types
    intptr_t, bool, std::pair<int, int>>;

using AnyMap = std::map<std::string, Any, std::less<void>>;

template <typename T>
inline T any_cast(const Any &any) {
    T result{};
    try {
        result = std::get<T>(any);
    } catch (const std::bad_variant_access &e) {
        std::cerr << e.what() << '\n';
    }
    return result;
}

template <typename T>
inline bool any_holds_type(const Any &any) {
    return std::holds_alternative<T>(any);
}

/**
 * @brief Dictionary is general-purpose key-value container.
 */
class Dictionary {
  public:
    virtual ~Dictionary(){};

    /**
     * @brief Returns dictionary name. Utility function find_metadata uses it to find metadata by specified name.
     */
    virtual std::string name() const = 0;

    /**
     * @brief Returns a vector of all the available keys in the dictionary.
     */
    virtual std::vector<std::string> keys() const = 0;

    /**
     * @brief Returns dictionary element by key. If no handle with the specified key, returns empty std::optional.
     * @param key the key of the dictionary element to find
     */
    virtual std::optional<Any> try_get(std::string_view key) const noexcept = 0;

    /**
     * @brief Returns memory buffer stored in dictionary. The first element of returned std::pair is pointer to memory
     * buffer, second element is buffer size in bytes. If no buffer with the specified key, returns {nullptr,0}.
     * @param key the key of the memory buffer to find
     */
    virtual std::pair<const void *, size_t> try_get_array(std::string_view key) const noexcept = 0;

    /**
     * @brief Adds element into the dictionary. If the dictionary already contains an element with specified key,
     * the existing value will be overwritten.
     * @param key the key of the element to add
     * @param value the value of the element to add
     */
    virtual void set(std::string_view key, Any value) = 0;

    /**
     * @brief Allocates memory buffer and copies data from buffer specified in function parameters, and adds memory
     * buffer to the dictionary. If the dictionary already contains an buffer with specified key, the existing buffer
     * will be overwritten.
     * @param key the key of the memory buffer to add
     * @param data pointer to the memory buffer to add
     * @param nbytes size (in bytes) of the memory buffer to add
     */
    virtual void set_array(std::string_view key, const void *data, size_t nbytes) = 0;

    /**
     * @brief Sets name of dictionary. The existing name will be overwritten.
     * @param name dictionary name
     */
    virtual void set_name(std::string const &name) = 0;

    template <typename T>
    inline T get(std::string_view key) const {
        std::optional<Any> opt_val = try_get(key);
        if (opt_val)
            return any_cast<T>(*opt_val);
        throw std::out_of_range("No dictionary element with key: " + std::string(key));
    }

    template <typename T>
    inline T get(std::string_view key, T default_value) const {
        std::optional<Any> opt_val = try_get(key);
        if (opt_val)
            return any_cast<T>(*opt_val);
        return default_value;
    }

    template <class T>
    const std::vector<T> get_array(std::string_view key) const {
        auto val = try_get_array(key);
        if (val.first) {
            const T *ptr_begin = static_cast<const T *>(val.first);
            const T *ptr_end = reinterpret_cast<const T *>(static_cast<const uint8_t *>(val.first) + val.second);
            return std::vector<T>(ptr_begin, ptr_end);
        } else {
            throw std::out_of_range("No dictionary element with key: " + std::string(key));
        }
    }
};

using DictionaryPtr = std::shared_ptr<Dictionary>;

using DictionaryCPtr = std::shared_ptr<const Dictionary>;

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

    std::optional<Any> try_get(std::string_view key) const noexcept override {
        return _dict->try_get(key);
    }

    std::pair<const void *, size_t> try_get_array(std::string_view key) const noexcept override {
        return _dict->try_get_array(key);
    }

    void set(std::string_view key, Any value) override {
        return _dict->set(key, value);
    }

    void set_array(std::string_view key, const void *data, size_t nbytes) override {
        return _dict->set_array(key, data, nbytes);
    }

    void set_name(std::string const &name) override {
        _dict->set_name(name);
    }

    std::vector<std::string> keys() const override {
        return _dict->keys();
    }

  protected:
    DictionaryPtr _dict;
};

} // namespace dlstreamer
