/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/element.h"
#include "dlstreamer/base/frame.h"
#include "dlstreamer/source.h"

namespace dlstreamer {

class BaseSource : public BaseElement<Source> {
  public:
    BaseSource(const ContextPtr &app_context) : _app_context(app_context) {
    }

    void set_output_info(const FrameInfo &info) override {
        _output_info = info;
    }

    FrameInfo get_output_info() override {
        return _output_info;
    }

  protected:
    ContextPtr _app_context;
    FrameInfo _output_info;
};

using BaseSourcePtr = std::shared_ptr<BaseSource>;

} // namespace dlstreamer
