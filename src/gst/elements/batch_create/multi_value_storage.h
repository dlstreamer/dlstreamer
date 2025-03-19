/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/dictionary.h"
#include "dlstreamer/element.h"
#include <gst/base/gstbasetransform.h>
#include <mutex>

namespace dlstreamer {

template <class Key, class Value>
class MultiValueStorage {
  public:
    void add(Key key, Value value) {
        std::lock_guard<std::mutex> guard(_mutex);
        _values[key].push_back(value);
    }
    void remove(Key key, Value value) {
        std::lock_guard<std::mutex> guard(_mutex);
        auto vec_it = _values.find(key);
        if (vec_it == _values.end())
            throw std::runtime_error("Key not found");
        auto &vec = vec_it->second;
        auto it = std::find(vec.begin(), vec.end(), value);
        if (it == vec.end())
            throw std::runtime_error("Value not found");
        vec.erase(it);
        if (vec.empty())
            _values.erase(vec_it);
    }
    Value get_first(Key key) {
        std::lock_guard<std::mutex> guard(_mutex);
        return *_values[key].begin();
    }

  private:
    std::map<Key, std::vector<Value>> _values;
    std::mutex _mutex;
};

} // namespace dlstreamer
