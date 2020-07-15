/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "utils.h"

#include <fstream>
#include <sstream>

namespace Utils {

std::string createNestedErrorMsg(const std::exception &e, int level) {
    static std::string msg = "\n";
    msg += std::string(level, ' ') + e.what() + "\n";
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        createNestedErrorMsg(e, level + 1);
    }
    return msg;
}

std::vector<std::string> splitString(const std::string &input, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool fileExists(const std::string &path) {
    return std::ifstream(path).good();
}

} // namespace Utils