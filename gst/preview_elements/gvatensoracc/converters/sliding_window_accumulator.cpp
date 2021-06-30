/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

/*
Description:
1. Push memories to queue
    * When N (window size) memories are accumulated accumulator is marked as ready
2. Get result from accumulated memories: merge all memories into resulting one
3. Accumulate K (window step) more memories
    * Acc Counter is used to track pushed memories
    * When new memory is pushed we pop old memory from queue's head
    * When Acc Counter is zero accumulator is marked as ready again (we accumulated K new memories)
4. Repeat from the 2nd step

Example:

Window size: N=10
Window step: K=3

accumulate: Queue: O                        Acc Counter: 0     Ready: False
accumulate: Queue: O O                      Acc Counter: 0     Ready: False
...
accumulate: Queue: O O O O O O O O O O      Acc Counter: 0     Ready: True
                    \______ N=10 _____|
get_result:  Queue: O O O O O O O O O O     Acc Counter: 3     Ready: False
accumulate: Queue: O O O O O O O O O O`     Acc Counter: 2     Ready: False
accumulate: Queue: O O O O O O O O O`O`     Acc Counter: 1     Ready: False
accumulate: Queue: O O O O O O O O`O`O`     Acc Counter: 0     Ready: True
*/

#include "sliding_window_accumulator.hpp"

#include <stdexcept>

SlidingWindowAccumulator::SlidingWindowAccumulator(guint window_size, guint window_step)
    : m_window_size(window_size), m_window_step(window_step) {
    if (m_window_size == 0 || m_window_step == 0)
        throw std::invalid_argument("Window size or step has invalid value");
    if (m_window_step > m_window_size)
        throw std::invalid_argument("Window size is less then window step");
}

SlidingWindowAccumulator::~SlidingWindowAccumulator() {
    clear();
}

void SlidingWindowAccumulator::accumulate(GstMemory *mem) {
    gst_memory_ref(mem);
    m_queue.push_back(mem);
    if (m_queue.size() < m_window_size)
        return;
    if (m_queue.size() > m_window_size)
        pop();

    if (m_acc_counter > 0)
        m_acc_counter--;
}

GstMemory *SlidingWindowAccumulator::get_result() {
    if (m_queue.size() >= m_window_size)
        m_acc_counter = m_window_step;
    return merge();
}

void SlidingWindowAccumulator::clear() {
    while (!m_queue.empty()) {
        pop();
    }
    m_acc_counter = 0;
}

void SlidingWindowAccumulator::pop() {
    auto mem = m_queue.front();
    gst_memory_unref(mem);
    m_queue.pop_front();
}

GstMemory *SlidingWindowAccumulator::merge() {
    /** TODO: Check if there any situation where:
     * accumulated embeddings share the memory with a common parent memory object
     * and that the memory is contiguous. So they can be merged efficiently by gst_memory_share.
     */
    GstMapInfo src_info, dst_info;
    /* TODO: do we always have memory of the same size in all incoming buffers ? */
    gsize size = m_window_size * m_queue.back()->size;
    GstMemory *result = gst_allocator_alloc(NULL, size, NULL);

    if (result == nullptr || !gst_memory_map(result, &dst_info, GST_MAP_WRITE)) {
        if (result) {
            gst_memory_unref(result);
        }
        return nullptr;
    }

    guint8 *ptr = dst_info.data;
    memset(ptr, (char)0, size);
    for (const auto &mem : m_queue) {
        if (!gst_memory_map(mem, &src_info, GST_MAP_READ)) {
            gst_memory_unmap(result, &dst_info);
            gst_memory_unref(result);
            return nullptr;
        }
        memcpy(ptr, (guint8 *)src_info.data, src_info.size);
        ptr += src_info.size;
        gst_memory_unmap(mem, &src_info);
    }
    gst_memory_unmap(result, &dst_info);

    return result;
}
