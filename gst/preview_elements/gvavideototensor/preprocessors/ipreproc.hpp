/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>

class IPreProc {
  public:
    virtual ~IPreProc() = default;

    virtual void process(GstBuffer *in_buffer, GstBuffer *out_buffer, GstVideoRegionOfInterestMeta *roi = nullptr) = 0;

    virtual void flush() = 0;

    virtual size_t output_size() const = 0;
    virtual bool need_preprocessing() const = 0;
};
