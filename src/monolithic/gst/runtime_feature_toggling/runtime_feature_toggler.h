/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "feature_toggling/ifeature_toggler.h"

#include <map>
#include <memory>
#include <type_traits>

namespace FeatureToggling {
namespace Runtime {

class RuntimeFeatureToggler : public Base::IFeatureToggler {
    std::map<std::string, bool> features;
    std::unique_ptr<Base::IOptionsReader> options_reader;

  public:
    RuntimeFeatureToggler() = default;

    void configure(const std::vector<std::string> &enabled_features);
    bool enabled(const std::string &id);
};

} // namespace Runtime
} // namespace FeatureToggling
