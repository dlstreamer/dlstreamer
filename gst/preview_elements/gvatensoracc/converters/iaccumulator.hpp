/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

class IAccumulator {
  public:
    virtual ~IAccumulator() = default;
    /**
     * @brief Accumulate memory
     *
     * @param mem
     */
    virtual void accumulate(GstMemory *mem) = 0;
    /**
     * @brief Resulting data from accumulated memories
     *
     * @return GstMemory* accumulation result
     */
    virtual GstMemory *get_result() = 0;
    /**
     * @brief Clear all memories in accumulator
     *
     */
    virtual void clear() = 0;
};
