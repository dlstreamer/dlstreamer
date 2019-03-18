/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "glib.h"
#include <gst/gst.h>

#ifndef GVA_UTILS_H
#define GVA_UTILS_H

#ifdef __cplusplus
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

__inline std::vector<std::string> SplitString(const std::string input, char delimiter = ',') {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

__inline std::string GetStringArrayElem(const std::string &in_str, int index) {
    auto tokens = SplitString(in_str);
    if (index < 0 || (size_t)index >= tokens.size())
        return "";
    return tokens[index];
}

__inline std::map<std::string, std::string> String2Map(std::string const &s) {
    std::string key, val;
    std::istringstream iss(s);
    std::map<std::string, std::string> m;

    while (std::getline(std::getline(iss, key, '=') >> std::ws, val)) {
        m[key] = val;
    }

    return m;
}
#endif

#endif
