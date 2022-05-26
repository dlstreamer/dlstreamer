/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gstbuffer.h>

class IFilter {
  public:
    virtual ~IFilter() = default;

    /**
     * @brief Do filtering on provided buffer
     */
    virtual void invoke(GstBuffer *buffer) = 0;
};
