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
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _MSC_VER
#define PRETTY_FUNCTION_NAME __FUNCSIG__
#else
#define PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

namespace Utils {

const std::string dpcppInstructionMsg = "Seems DPC++ dependency is not installed. Please follow installation guide: "
                                        "https://github.com/openvinotoolkit/dlstreamer_gst/wiki/DPCPP-Install-Guide";

std::string createNestedErrorMsg(const std::exception &e, std::string &&msg = "", int level = 0);
std::vector<std::string> splitString(const std::string &input, char delimiter = ',');

template <typename Iter>
std::string join(Iter begin, Iter end, char delimiter = ',') {
    std::ostringstream result;
    for (auto iter = begin; iter != end; iter++) {
        if (iter == begin)
            result << *iter;
        else
            result << delimiter << *iter;
    }
    return result.str();
}

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
constexpr bool IsLinux() {
#ifdef __linux__
    return true;
#else
    return false;
#endif
}
size_t GetFileSize(const std::string &file_path);
bool CheckFileSize(const std::string &path, size_t size_threshold);
std::tuple<bool, std::string> parseDeviceName(const std::string &device_name);

/**
 * Parses GPU device name and returns relative GPU index from it.
 *
 * @param[in] device GPU device name in forms: GPU, GPU.0, etc.
 *
 * @return relative GPU index.
 */
uint32_t getRelativeGpuDeviceIndex(const std::string &device);

/**
 * Converts string to boolean value.
 * Valid values are: 1, 0, true, false
 *
 * @param s string to convert.
 * @return boolean value.
 * @exception std::invalid_argument if no conversion can be performed.
 */
bool strToBool(const std::string &s);

/**
 * Returns number of planes for specified format.
 *
 * @param fourcc data format as FourCC value.
 * @return number of planes.
 */
uint32_t GetPlanesCount(int fourcc) noexcept;

/**
 * Checks if all keys in config are known.
 *
 * @param known_keys set of known keys.
 * @param config configuration map whose keys should be checked.
 * @return true if all config keys are from the set of known keys.
 */
bool checkAllKeysAreKnown(const std::set<std::string> &known_keys, const std::map<std::string, std::string> &config);

/**
 * @return the process ID (PID) of the calling process.
 */
int getProcessId();

/**
 * Searches for processes which have an open file descriptor to the given file.
 *
 * @param file_name path to a file
 * @param access_mode can be "w", "r"
 * @return count of processes which access the file with the given access mode
 */
int getOpenedByProcessesDescriptorsCount(const std::string &file_name, const std::string &access_mode);

/*
 * @brief Trims path and replaces leading ~ symbol with the HOME path
 *
 * @param path
 * @return correct absolute path
 */
std::string fixPath(std::string path);

} // namespace Utils

#endif
