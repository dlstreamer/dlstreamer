/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#define UNUSED(x) ((void)(x))

#ifdef __cplusplus

#include <exception>
#include <map>
#include <string>
#include <vector>

namespace Utils {

std::string createNestedErrorMsg(const std::exception &e, int level = 0, std::string &&msg = "");
std::vector<std::string> splitString(const std::string &input, char delimiter = ',');
/**
 * Converts string in format `key1=val1,key2=val2,...` to key/value pairs.
 *
 * @param[in] s string to convert.
 * @param[in] rec_delim delimiter to use between records (by default - `,`).
 * @param[in] kv_delim delimiter to use between key and value (by default - `=`).
 *
 * @return map container with key/value pairs of strings.
 */
std::map<std::string, std::string> stringToMap(const std::string &s, char rec_delim = ',', char kv_delim = '=');
bool fileExists(const std::string &path);
std::tuple<bool, std::string> parseDeviceName(const std::string &device_name);

/**
 * Converts string to boolean value.
 * Valid values are: 1, 0, true, false
 *
 * @param s string to convert.
 * @return boolean value.
 * @exception std::invalid_argument if no conversion can be performed.
 */
bool strToBool(const std::string &s);

} // namespace Utils

#endif
