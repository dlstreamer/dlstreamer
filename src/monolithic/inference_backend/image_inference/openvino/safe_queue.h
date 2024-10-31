/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/logger.h"
#include <condition_variable>
#include <mutex>
#include <queue>

template <class T>
class SafeQueue {
  public:
    void push(T t) {
        ITT_TASK("SafeQueue::push");
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_back(t);
        }
        condition_.notify_one();
    }

    void push_front(T t) {
        ITT_TASK("SafeQueue::push_front");
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push_front(t);
        }
        condition_.notify_one();
    }

    T &front() {
        ITT_TASK("SafeQueue::front");
        std::unique_lock<std::mutex> lock(mutex_);
        while (queue_.empty()) {
            condition_.wait(lock);
        }
        return queue_.front();
    }

    T pop() {
        ITT_TASK("SafeQueue::pop");
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
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void waitEmpty() {
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
