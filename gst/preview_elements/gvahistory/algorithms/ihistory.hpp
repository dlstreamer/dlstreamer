/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gstbuffer.h>

class IHistory {
  public:
    virtual ~IHistory() = default;

    /**
     * @brief Processing the provided buffer
     *
     * @param buffer
     * @return true - buffer should be pushed further
     * @return false - buffer should be dropped
     */
    virtual bool invoke(GstBuffer *buffer) = 0;
};
