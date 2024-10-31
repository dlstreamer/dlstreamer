/*******************************************************************************
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer_logger.h"
#include <condition_variable>
#include <mutex>
#include <queue>

namespace dlstreamer {
template <class T>
class SafeQueue {
  public:
    void push(T t) {
        auto task = itt::Task("infer_requests_queue:SafeQueue:push");
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_back(t);
        }
        condition_.notify_one();
    }

    void push_front(T t) {
        auto task = itt::Task("infer_requests_queue:SafeQueue:push_front");
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_front(t);
        }
        condition_.notify_one();
    }

    T &front() {
        auto task = itt::Task("infer_requests_queue:SafeQueue:front");
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {
            condition_.wait(lock);
        }
        return queue_.front();
    }

    T pop() {
        auto task = itt::Task("infer_requests_queue:SafeQueue:pop");
        T value;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            while (queue_.empty()) {
                condition_.wait(lock);
            }
            value = queue_.front();
            queue_.pop_front();
        }
        condition_.notify_one();
        return value;
    }

    bool empty() {
        auto task = itt::Task("infer_requests_queue:SafeQueue:empty");
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void waitEmpty() {
        auto task = itt::Task("infer_requests_queue:SafeQueue:waitEmpty");
        std::unique_lock<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            condition_.wait(lock);
        }
    }

  private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
};

} // namespace dlstreamer
