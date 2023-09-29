/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <string>

namespace FeatureToggling {
namespace Base {

struct IFeatureToggleVirtual {
    virtual ~IFeatureToggleVirtual() = default;
    virtual const std::string &id() const = 0;
    virtual const std::string &deprecationMessage() const = 0;
};

template <typename ToggleType>
struct IFeatureToggle : IFeatureToggleVirtual {
    const std::string &id() const override {
        return ToggleType::id;
    }

    const std::string &deprecationMessage() const override {
        return ToggleType::deprecation_message;
    }
};

} // namespace Base
} // namespace FeatureToggling

#define CREATE_FEATURE_TOGGLE(TOGGLE_TYPE, FEATURE_TOGGLE_ID, FEATURE_TOGGLE_DEPRECATION_MESSAGE)                      \
    struct TOGGLE_TYPE final : FeatureToggling::Base::IFeatureToggle<TOGGLE_TYPE> {                                    \
        static const std::string id;                                                                                   \
        static const std::string deprecation_message;                                                                  \
    };                                                                                                                 \
    const std::string TOGGLE_TYPE::id = FEATURE_TOGGLE_ID;                                                             \
    const std::string TOGGLE_TYPE::deprecation_message = FEATURE_TOGGLE_DEPRECATION_MESSAGE;
