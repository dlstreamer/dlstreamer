/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace dlstreamer {

template <typename T>
class Pool {
  public:
    virtual ~Pool(){};

    virtual T get_or_create() = 0;

    virtual size_t size() const = 0;
};

// Pool of shared_ptr objects
template <typename T>
class PoolSharedPtr : public Pool<T> {
  public:
    PoolSharedPtr(std::function<T()> allocator, size_t max_pool_size = 0)
        : _allocator(allocator), _max_pool_size(max_pool_size) {
    }

    T get_or_create() override {
        std::lock_guard<std::mutex> lock(_mutex);
        for (;;) {
            for (const T &object : _pool) {
                if (object.use_count() == 1) { // use_count() is std::shared_ptr function
                    return object;
                }
            }
            if (!_max_pool_size || _pool.size() < _max_pool_size) { // allocate new object
                T object = _allocator();
                _pool.push_back(object);
                return object;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // TODO optimize
        }
    }

    size_t size() const override {
        return _pool.size();
    }

  private:
    std::function<T()> _allocator;
    std::vector<T> _pool;
    std::mutex _mutex;
    size_t _max_pool_size = 0;
};

} // namespace dlstreamer
