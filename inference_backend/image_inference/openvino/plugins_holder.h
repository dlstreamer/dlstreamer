/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include <inference_engine.hpp>
#include <mutex>

class PluginsHolderSingleton {
  public:
    static PluginsHolderSingleton &getInstance();

    PluginsHolderSingleton(const PluginsHolderSingleton &) = delete;
    PluginsHolderSingleton(PluginsHolderSingleton &&) = delete;
    PluginsHolderSingleton &operator=(const PluginsHolderSingleton &) = delete;
    PluginsHolderSingleton &operator=(PluginsHolderSingleton &&) = delete;

    InferenceEngine::InferencePlugin::Ptr getPluginPtr(const std::string &deviceName,
                                                       const std::string &pluginsDir = "");

  private:
    PluginsHolderSingleton() = default;

    std::map<std::string, std::weak_ptr<InferenceEngine::InferencePlugin>> plugins = {};
    std::mutex mutex;
};
