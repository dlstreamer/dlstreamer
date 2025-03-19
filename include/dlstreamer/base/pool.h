/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace dlstreamer {

template <typename T>
class Pool {
  public:
    Pool(std::function<T()> allocator, std::function<bool(T &)> is_available, size_t max_pool_size = 0)
        : _allocator(allocator), _is_available(is_available), _max_pool_size(max_pool_size) {
    }

    T get_or_create() {

        for (;;) {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                for (T &object : _pool) {
                    if (_is_available(object))
                        return object;
                }
                if (!_max_pool_size || _pool.size() < _max_pool_size) { // allocate new object
                    T object = _allocator();
                    _pool.push_back(object);
                    return object;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // TODO optimize
        }
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _pool.size();
    }

  private:
    std::function<T()> _allocator;
    std::function<bool(T &)> _is_available;
    std::vector<T> _pool;
    std::mutex _mutex;
    size_t _max_pool_size = 0;
};

} // namespace dlstreamer
