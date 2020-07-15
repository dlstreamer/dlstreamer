/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <exception>
#include <string>
#include <vector>

namespace Utils {

std::string createNestedErrorMsg(const std::exception &e, int level = 0);
std::vector<std::string> splitString(const std::string &input, char delimiter = ',');
bool fileExists(const std::string &path);

} // namespace Utils