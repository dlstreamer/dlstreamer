/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gtest/gtest.h>

// In can be inherited from ::testing::Environment but it gives almost nothing
class TestEnv {
  public: /* static */
    static std::string getModelPath(const std::string &modelName, const std::string &precision);
    static std::string getModelProcPath(const std::string &modelName);

  private:
    // Single global instance
    static TestEnv _instance;

    std::vector<std::string> _pathsToModels;
    std::vector<std::string> _pathsToModelProcs;

    void getModelPathInternal(const std::string &modelName, const std::string &precision, std::string &resultPath);
    void getModelProcPathInternal(const std::string &modelName, std::string &resultPath);
    void initPathsToModels();
    void initPathsToModelProcs();
};
