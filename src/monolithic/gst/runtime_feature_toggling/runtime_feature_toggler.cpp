/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "runtime_feature_toggler.h"

using namespace FeatureToggling;
using namespace Runtime;

void RuntimeFeatureToggler::configure(const std::vector<std::string> &enabled_features) {
    for (auto feature_name : enabled_features) {
        features.emplace(feature_name, true);
    }
}

bool RuntimeFeatureToggler::enabled(const std::string &id) {
    auto it = features.find(id);
    if (it == features.end()) {
        std::tie(it, std::ignore) = features.emplace(id, false);
    }
    return it->second;
}
