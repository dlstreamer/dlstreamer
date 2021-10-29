/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stack>
#include <string>

#include "tensor_inference.hpp"

/* test out if spin lock more suitable and more efficient in some cases, e.g. for inference queue */
class spin_lock {
    std::atomic_flag locked = ATOMIC_FLAG_INIT;

  public:
    void lock() {
        while (locked.test_and_set(std::memory_order_acquire)) {
        }
    }
    void unlock() {
        locked.clear(std::memory_order_release);
    }
};

/**
 * TODO: class' description and methods' description
 * TODO: maybe rename class
 */
class InferenceInstances {
  private:
    using value_type = std::shared_ptr<TensorInference>;
    using map_type = std::map<std::string, value_type>;

  public:
    InferenceInstances() = delete;

    static value_type get(const std::string &instance_id, const std::string &model) {
        if (!_instances.count(instance_id)) {
            std::lock_guard<std::mutex> lock(_mutex);
            _instances[instance_id] = std::make_shared<TensorInference>(model);
        }
        return _instances[instance_id];
    }

  private:
    static map_type _instances;

    static std::mutex _mutex;
};

/**
 * TODO: class' description and methods' description
 * TODO: maybe rename class
 */
template <typename T>
class InferenceQueue {
  private:
    using value_type = T;
    using queue_type = std::list<value_type>;

  public:
    void push(const value_type &value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(value);
    }

    queue_type shrink(std::function<bool(const value_type &)> is_ready) {
        std::lock_guard<std::mutex> lock(_mutex);
        queue_type result;
        auto it = std::find_if(_queue.begin(), _queue.end(), [is_ready](const value_type &v) { return !is_ready(v); });
        result.splice(result.end(), _queue, _queue.begin(), it);
        return result;
    }

    const T &back() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.back();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

  private:
    queue_type _queue;

    mutable std::mutex _mutex;
};

/**
 * TODO: class' description and methods' description
 * TODO: maybe rename class
 */
class MemoryPool {
  public:
    using value_type = uint8_t;
    using reference = value_type &;
    using pointer = value_type *;
    using pool_type = std::stack<std::unique_ptr<value_type>>;
    using size_type = std::size_t;

    MemoryPool(size_type chunk_size, size_type size) : _chunk_size(chunk_size) {
        if (_chunk_size == 0)
            throw std::invalid_argument("chunk_size cannot be null");
        internal_reserve(size);
    }

    ~MemoryPool() {
    }

    size_type size() {
        return _size;
    }

    size_type chunk_size() {
        return _chunk_size;
    }

    pointer acquire() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_stack.empty()) {
            // TODO: refactor new allocation size
            auto new_size = _size + std::max(size_t(1), static_cast<size_t>(_size / 3));
            internal_reserve(new_size);
        }
        assert(!_stack.empty());
        pointer chunk = _stack.top().release();
        _stack.pop();
        return chunk;
    }

    void release(pointer value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _stack.emplace(value);
    }

  private:
    pool_type _stack;
    size_type _chunk_size;
    size_type _size = 0;

    std::mutex _mutex;

    void internal_reserve(size_type new_size) {
        if (new_size <= _size)
            return;

        for (size_type i = _size; i < new_size; ++i) {
            pool_type::value_type mem(new value_type[_chunk_size]);
            _stack.emplace(std::move(mem));
        }
        _size = new_size;
    }
};

template <typename T>
class SmartWrapper {
  private:
    using value_type = T;
    using reference = value_type &;
    using pointer = value_type *;
    using deleter_type = std::function<void(reference)>;

  public:
    SmartWrapper(const value_type &value) : _value(value) {
    }
    SmartWrapper(const value_type &value, deleter_type deleter) : _value(value), _deleter(deleter) {
    }
    ~SmartWrapper() {
        _deleter(_value);
    }

  private:
    value_type _value;
    deleter_type _deleter;
};
