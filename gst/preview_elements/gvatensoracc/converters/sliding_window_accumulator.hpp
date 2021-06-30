/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "iaccumulator.hpp"

#include <deque>

class SlidingWindowAccumulator : public IAccumulator {
  public:
    SlidingWindowAccumulator(guint window_size, guint window_step);
    ~SlidingWindowAccumulator() override;

    void accumulate(GstMemory *mem) override;
    GstMemory *get_result() override;
    void clear() override;

  private:
    guint m_window_size;
    guint m_window_step;
    guint m_acc_counter = 0;
    std::deque<GstMemory *> m_queue;

    void pop();
    GstMemory *merge();
};
