/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "iaccumulator.hpp"

#include <safe_arithmetic.hpp>

#include <deque>
#include <stdexcept>

/**
 * Implements sliding windows algorithm for accumulating buffer's data.
 * `public` methods have generic implementation.
 * `private` methods have specialized implementation
 * and must be separately implemented for each T type.
 *
 * Description:
 * 1. Push memories to queue
 *     * When N (window size) memories are accumulated accumulator is marked as ready
 * 2. Get result from accumulated memories: merge all memories into resulting one
 * 3. Accumulate K (window step) more memories
 *     * Acc Counter is used to track pushed memories
 *     * When new memory is pushed we pop old memory from queue's head
 *     * When Acc Counter is zero accumulator is marked as ready again (we accumulated K new memories)
 * 4. Repeat from the 2nd step
 *
 * Example:
 *
 * Window size: N=10
 * Window step: K=3
 *
 * accumulate: Queue: O                        Acc Counter: 0     Ready: False
 * accumulate: Queue: O O                      Acc Counter: 0     Ready: False
 * ...
 * accumulate: Queue: O O O O O O O O O O      Acc Counter: 0     Ready: True
 *                     \______ N=10 _____|
 * get_result:  Queue: O O O O O O O O O O     Acc Counter: 3     Ready: False
 * accumulate: Queue: O O O O O O O O O O`     Acc Counter: 2     Ready: False
 * accumulate: Queue: O O O O O O O O O`O`     Acc Counter: 1     Ready: False
 * accumulate: Queue: O O O O O O O O`O`O`     Acc Counter: 0     Ready: True
 */
template <typename T>
class SlidingWindowAccumulator : public IAccumulator {
  public:
    using value_type = T;
    using reference = value_type &;
    using pointer = value_type *;

    SlidingWindowAccumulator(guint window_size, guint window_step);
    ~SlidingWindowAccumulator() override;

    void accumulate(GstBuffer *inbuf) override;
    bool get_result(GstBuffer *outbuf) override;

    void clear() override;

  private:
    guint m_window_size;
    guint m_window_step;
    guint m_acc_counter = 0;
    std::deque<pointer> m_queue;

    void pop();
    void accumulate_internal(GstBuffer *inbuf);
    bool merge(GstBuffer *outbuf);
};

template <typename T>
SlidingWindowAccumulator<T>::SlidingWindowAccumulator(guint window_size, guint window_step)
    : m_window_size(window_size), m_window_step(window_step) {
    if (m_window_size == 0 || m_window_step == 0)
        throw std::invalid_argument("Window size or step has invalid value");
    if (m_window_step > m_window_size)
        throw std::invalid_argument("Window size is less then window step");
}

template <typename T>
SlidingWindowAccumulator<T>::~SlidingWindowAccumulator() {
    clear();
}

template <typename T>
void SlidingWindowAccumulator<T>::accumulate(GstBuffer *inbuf) {
    accumulate_internal(inbuf);
}

template <typename T>
bool SlidingWindowAccumulator<T>::get_result(GstBuffer *outbuf) {
    if (m_queue.size() >= m_window_size)
        m_acc_counter = m_window_step;
    return merge(outbuf);
}

template <typename T>
void SlidingWindowAccumulator<T>::clear() {
    while (!m_queue.empty()) {
        pop();
    }
    m_acc_counter = 0;
}

/* ------------------------------- internal (GstMemory) ------------------------------- */

template <>
void SlidingWindowAccumulator<GstMemory>::pop() {
    auto mem = m_queue.front();
    gst_memory_unref(mem);
    m_queue.pop_front();
}

template <>
void SlidingWindowAccumulator<GstMemory>::accumulate_internal(GstBuffer *inbuf) {
    if (!inbuf)
        throw std::invalid_argument("GstBuffer is invalid");

    pointer mem = gst_buffer_peek_memory(inbuf, 0);
    gst_memory_ref(mem);
    m_queue.push_back(mem);
    if (m_queue.size() < m_window_size)
        return;
    if (m_queue.size() > m_window_size)
        pop();

    if (m_acc_counter > 0)
        m_acc_counter--;
}

template <>
bool SlidingWindowAccumulator<GstMemory>::merge(GstBuffer *outbuf) {
    /** TODO: Check if there any situation where:
     * accumulated embeddings share the memory with a common parent memory object
     * and that the memory is contiguous. So they can be merged efficiently by gst_memory_share.
     */
    GstMapInfo src_info, dst_info;
    /* TODO: do we always have memory of the same size in all incoming buffers ? */
    gsize size = safe_mul<gsize>(m_window_size, m_queue.back()->size);
    GstMemory *result = gst_allocator_alloc(NULL, size, NULL);

    if (result == nullptr || !gst_memory_map(result, &dst_info, GST_MAP_WRITE)) {
        if (result) {
            gst_memory_unref(result);
        }
        return false;
    }

    guint8 *ptr = dst_info.data;
    memset(ptr, 0, size);
    for (const auto &mem : m_queue) {
        if (!gst_memory_map(mem, &src_info, GST_MAP_READ)) {
            gst_memory_unmap(result, &dst_info);
            gst_memory_unref(result);
            return false;
        }
        memcpy(ptr, src_info.data, src_info.size);
        ptr += src_info.size;
        gst_memory_unmap(mem, &src_info);
    }
    gst_memory_unmap(result, &dst_info);

    gst_buffer_append_memory(outbuf, result);
    return true;
}

/* ------------------------------- internal (GstMeta) ------------------------------- */

template <>
void SlidingWindowAccumulator<GstMeta>::pop() {
    throw std::logic_error("Not implemented yet.");
}

template <>
void SlidingWindowAccumulator<GstMeta>::accumulate_internal(GstBuffer *) {
    throw std::logic_error("Not implemented yet.");
}

template <>
bool SlidingWindowAccumulator<GstMeta>::merge(GstBuffer *) {
    throw std::logic_error("Not implemented yet.");
}
