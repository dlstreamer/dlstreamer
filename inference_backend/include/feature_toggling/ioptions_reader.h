/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <string>
#include <vector>

namespace FeatureToggling {
namespace Base {

class IOptionsReader {
  public:
    virtual ~IOptionsReader() = default;
    virtual std::vector<std::string> read(const std::string &source) = 0;
};

} // namespace Base
} // namespace FeatureToggling
