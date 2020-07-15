/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "feature_toggling/ioptions_reader.h"

#include "utils.h"

#include <cstdlib>

namespace FeatureToggling {
namespace Runtime {

class EnvironmentVariableOptionsReader : public Base::IOptionsReader {
  public:
    ~EnvironmentVariableOptionsReader() = default;
    std::vector<std::string> read(const std::string &source) {
        std::vector<std::string> features;
        char *env = std::getenv(source.c_str());
        if (env)
            features = Utils::splitString(env, ',');
        return features;
    }
};

} // namespace Runtime
} // namespace FeatureToggling
