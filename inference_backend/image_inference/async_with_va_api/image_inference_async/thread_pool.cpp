/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "thread_pool.h"

ThreadPool::ThreadPool(size_t size) {
    for (size_t i = 0; i < size; ++i) {
        _threads.emplace_back(&ThreadPool::_task_runner, this);
    }
}

ThreadPool::~ThreadPool() {
    _terminate = true;
    _condition_variable.notify_all();
    for (auto &t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

std::future<void> ThreadPool::schedule(const std::function<void()> &callable) {
    std::shared_ptr<std::promise<void>> p = std::make_shared<std::promise<void>>();
    std::future<void> future = p->get_future();
    _tasks.push([callable, p]() {
        callable();
        p->set_value();
    });
    _condition_variable.notify_one();
    return future;
}

void ThreadPool::_task_runner() {
    while (!_terminate) {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition_variable.wait_for(lock, std::chrono::seconds(1),
                                     [this]() -> bool { return !_tasks.empty() || _terminate; });
        if (!_tasks.empty()) {
            auto task = _tasks.front();
            _tasks.pop();
            task();
        }
    }
}
