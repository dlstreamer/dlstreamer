/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "ioptions_reader.h"

#include <memory>
#include <string>

namespace FeatureToggling {
namespace Base {

struct IFeatureToggler {
    /**
     * @brief Configures toggler from list of enabled features
     * @param enabled_features const std::vector<std::string> & list of enabled features
     */
    virtual void configure(const std::vector<std::string> &enabled_features) = 0;

    virtual bool enabled(const std::string &id) = 0;
    virtual ~IFeatureToggler() = default;
};

} // namespace Base
} // namespace FeatureToggling
