/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <capabilities/types.hpp>
#include <frame_data.hpp>

class IPreProc {
  public:
    virtual ~IPreProc() = default;

    virtual void process(GstBuffer *in_buffer, GstBuffer *out_buffer) = 0;
    virtual void process(GstBuffer *buffer) = 0;

    virtual void flush() = 0;

    virtual size_t output_size() const = 0;
    virtual bool need_preprocessing() const = 0;
};
