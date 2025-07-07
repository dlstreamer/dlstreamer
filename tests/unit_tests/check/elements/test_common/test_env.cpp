/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "test_env.h"

#include <glob.h>

#include <ftw.h>
#include <utils.h>

TestEnv TestEnv::_instance;

namespace {

std::string findModel(const std::string &path, const std::string &name, const std::string &precision) {
    struct SearchData {
        // Input
        std::string name;
        std::string precision;

        // Out
        std::string path;

        void init(const std::string &name, const std::string &precision) {
            this->name = name;
            this->precision = precision;
            this->path.clear();
        }
    } static thread_local searchData;

    // Initialize search parameters
    searchData.init(name, precision);

    auto callback = [](const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) -> int {
        if (typeflag != FTW_F)
            return 0;

        if (!strstr(fpath + ftwbuf->base, searchData.name.c_str()))
            return 0;

        if (!searchData.precision.empty() && !strstr(fpath, searchData.precision.c_str()))
            return 0;

        searchData.path = fpath;
        return 1;
    };
    nftw(path.c_str(), callback, 15, FTW_PHYS);

    return searchData.path;
}

} // namespace

std::string TestEnv::getModelPath(const std::string &modelName, const std::string &precision) {
    std::string modelPath;
    _instance.getModelPathInternal(modelName + ".xml", precision, modelPath);
    return modelPath;
}

void TestEnv::getModelPathInternal(const std::string &modelName, const std::string &precision,
                                   std::string &resultPath) {
    if (_pathsToModels.empty())
        initPathsToModels();

    for (const auto &path : _pathsToModels) {
        resultPath = findModel(path, modelName, precision);
        if (!resultPath.empty())
            break;
    }

    ASSERT_FALSE(resultPath.empty()) << "Could not find model '" << modelName << "' with precision '" << precision
                                     << '\'';
}

void TestEnv::initPathsToModels() {
    ASSERT_NE(getenv("MODELS_PATH"), nullptr) << "The test require 'MODELS_PATH' environment variable to be set";
    const char *str = getenv("MODELS_PATH");

    _pathsToModels = Utils::splitString(str, ':');
}

std::string TestEnv::getModelProcPath(const std::string &modelName) {
    std::string modelProcPath;
    _instance.getModelProcPathInternal(modelName, modelProcPath);
    return modelProcPath;
}

void TestEnv::getModelProcPathInternal(const std::string &modelName, std::string &resultPath) {
    if (_pathsToModelProcs.empty())
        initPathsToModelProcs();

    for (const auto &path : _pathsToModelProcs) {
        resultPath = findModel(path, modelName + ".json", {});
        if (!resultPath.empty())
            break;
    }

    ASSERT_FALSE(resultPath.empty()) << "Could not find model-proc for model '" << modelName << '\'';
}

void TestEnv::initPathsToModelProcs() {
    ASSERT_NE(getenv("MODELS_PROC_PATH"), nullptr)
        << "The test require 'MODELS_PROC_PATH' environment variable to be set";
    const char *str = getenv("MODELS_PROC_PATH");

    _pathsToModelProcs = Utils::splitString(str, ':');
}
