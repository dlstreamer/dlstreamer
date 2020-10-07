/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

struct ITracker {
    virtual ~ITracker() = default;
    virtual void track(GstBuffer *buffer) = 0;
};
