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
};
