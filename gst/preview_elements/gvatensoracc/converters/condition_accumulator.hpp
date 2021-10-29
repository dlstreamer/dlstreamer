/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "iaccumulator.hpp"

#include <meta/gva_buffer_flags.hpp>

#include <deque>
#include <stdexcept>

/**
 * Implements event driven accumulating algorithm buffer's data.
 * All methods except `merge` have generic implementation.
 * `merge` method have specialized implementation
 * and must be separately implemented for each T type.
 */
template <typename T>
class ConditionAccumulator : public IAccumulator {
  public:
    using value_type = T;
    using reference = value_type &;
    using pointer = value_type *;

    ConditionAccumulator();
    ~ConditionAccumulator() override;

    void accumulate(GstBuffer *inbuf) override;
    bool get_result(GstBuffer *outbuf) override;

    void clear() override;

  private:
    std::deque<GstBuffer *> m_queue;
    bool m_ready;

    void pop();
    bool merge(GstBuffer *outbuf);
};

template <typename T>
ConditionAccumulator<T>::ConditionAccumulator() : m_ready(false) {
}

template <typename T>
ConditionAccumulator<T>::~ConditionAccumulator() {
    clear();
}

template <typename T>
void ConditionAccumulator<T>::accumulate(GstBuffer *inbuf) {
    gst_buffer_ref(inbuf);
    m_queue.push_back(inbuf);

    m_ready = gst_buffer_has_flags(inbuf, (GstBufferFlags)GVA_BUFFER_FLAG_LAST_ROI_ON_FRAME);
}

template <typename T>
bool ConditionAccumulator<T>::get_result(GstBuffer *outbuf) {
    if (!m_ready) {
        return false;
    }
    return merge(outbuf);
}

template <typename T>
void ConditionAccumulator<T>::clear() {
    while (!m_queue.empty()) {
        pop();
    }
}

template <typename T>
void ConditionAccumulator<T>::pop() {
    auto buff = m_queue.front();
    gst_buffer_unref(buff);
    m_queue.pop_front();
}

/* ------------------------------- internal (GstMemory) ------------------------------- */

template <>
bool ConditionAccumulator<GstMemory>::merge(GstBuffer *) {
    throw std::logic_error("Not implemented yet.");
}

/* ------------------------------- internal (GstMeta) ------------------------------- */

template <>
bool ConditionAccumulator<GstMeta>::merge(GstBuffer *outbuf) {
    /* using functor to simplify the code */
    auto copy_meta = [](GstBuffer *src, GstBuffer *dst) -> bool {
        GstMeta *meta;
        gpointer state = NULL;
        while ((meta = gst_buffer_iterate_meta(src, &state))) {
            GstMetaTransformCopy copy_data = {.region = false, .offset = 0, .size = static_cast<gsize>(-1)};
            if (!meta->info->transform_func(dst, meta, src, _gst_meta_transform_copy, &copy_data)) {
                GST_ERROR("Failed to copy metadata to out buffer");
                return false;
            }
        }
        return true;
    };

    while (!m_queue.empty()) {
        auto inbuf = m_queue.front();
        if (!copy_meta(inbuf, outbuf)) {
            return false;
        }
        pop();
    }

    m_ready = false;
    return true;
}
