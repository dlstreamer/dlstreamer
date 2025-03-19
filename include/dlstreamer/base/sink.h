/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/element.h"
#include "dlstreamer/base/frame.h"
#include "dlstreamer/sink.h"

namespace dlstreamer {

class BaseSink : public BaseElement<Sink> {
  public:
    BaseSink(const ContextPtr &app_context) : _app_context(app_context) {
    }

    void set_input_info(const FrameInfo &info) override {
        _input_info = info;
    }

    FrameInfo get_input_info() override {
        return _input_info;
    }

  protected:
    ContextPtr _app_context;
    FrameInfo _input_info;
};

using BaseSinkPtr = std::shared_ptr<BaseSink>;

} // namespace dlstreamer
