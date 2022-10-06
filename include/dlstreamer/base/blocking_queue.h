/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace dlstreamer {

template <typename T>
class BlockingQueue {
  public:
    void push(T const &value, size_t queue_limit = 0) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (queue_limit > 0) {
            while (_queue.size() >= queue_limit) {
                _pop_condition.wait(lock);
            }
        }
        _queue.push_front(value);
        _push_condition.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(_mutex);
        _push_condition.wait(lock, [=] { return !_queue.empty(); });
        T value(std::move(_queue.back()));
        _queue.pop_back();
        _pop_condition.notify_one();
        return value;
    }

    void clear() {
        _queue.clear();
        _pop_condition.notify_one();
    }

    size_t size() {
        return _queue.size();
    }

  private:
    std::deque<T> _queue;
    std::mutex _mutex;
    std::condition_variable _push_condition;
    std::condition_variable _pop_condition;
};

} // namespace dlstreamer
