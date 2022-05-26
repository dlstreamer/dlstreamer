/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <string>
#include <vector>

namespace dlstreamer {

class Context {
  public:
    virtual ~Context(){};
    virtual void *handle(std::string const &key) const = 0;
    virtual std::vector<std::string> keys() const = 0;
};

using ContextPtr = std::shared_ptr<Context>;

} // namespace dlstreamer
