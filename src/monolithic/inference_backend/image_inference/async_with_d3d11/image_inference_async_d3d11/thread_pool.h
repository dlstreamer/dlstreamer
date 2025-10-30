/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <queue>
#include <thread>

namespace InferenceBackend {
namespace D3D11 {

class ThreadPool {
  private:
    std::vector<std::thread> _threads;
    std::queue<std::function<void()>> _tasks;
    std::condition_variable _condition_variable;
    std::mutex _mutex;
    bool _terminate = false;

    void _task_runner();

  public:
    explicit ThreadPool(size_t size);

    ~ThreadPool();

    std::future<void> schedule(const std::function<void()> &callable);
};

} // namespace D3D11
} // namespace InferenceBackend
