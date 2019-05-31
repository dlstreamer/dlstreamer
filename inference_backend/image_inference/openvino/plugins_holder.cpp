/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "plugins_holder.h"

PluginsHolderSingleton &PluginsHolderSingleton::getInstance() {
    static PluginsHolderSingleton instance;
    return instance;
}

InferenceEngine::InferencePlugin::Ptr PluginsHolderSingleton::getPluginPtr(const std::string &deviceName,
                                                                           const std::string &pluginsDir) {
    std::lock_guard<std::mutex> lock(mutex);

    if (plugins.count(deviceName) == 0 || plugins.at(deviceName).expired()) {
        plugins.erase(deviceName);
        InferenceEngine::InferencePlugin::Ptr shared = std::make_shared<InferenceEngine::InferencePlugin>(
            InferenceEngine::PluginDispatcher({pluginsDir}).getPluginByDevice(deviceName));
        plugins[deviceName] = shared;
        return std::move(shared);
    }

    return plugins.at(deviceName).lock();
}
