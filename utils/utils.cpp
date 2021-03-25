/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "utils.h"

#include <cassert>
#include <fstream>
#include <regex>
#include <sstream>
#include <tuple>

namespace Utils {

std::string createNestedErrorMsg(const std::exception &e, int level, std::string &&msg) {
    msg += std::string(level, '\t') + e.what() + "\n";
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        msg = createNestedErrorMsg(e, ++level, std::move(msg));
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

std::map<std::string, std::string> stringToMap(const std::string &s, char rec_delim /*= ','*/,
                                               char kv_delim /*= '='*/) {
    std::string key, val;
    std::istringstream iss(s);
    std::map<std::string, std::string> m;

    while (std::getline(std::getline(iss, key, kv_delim) >> std::ws, val, rec_delim)) {
        m.emplace(std::move(key), std::move(val));
    }

    return m;
}

bool fileExists(const std::string &path) {
    return std::ifstream(path).good();
}

std::tuple<bool, std::string> parseDeviceName(const std::string &device_name) {
    bool has_vpu_device_id = false;
    std::string vpu_device_name;
    if (device_name.find("VPUX") == 0) {
        vpu_device_name = "VPU-0";
        if (device_name != "VPUX") {
            has_vpu_device_id = true;
            const std::regex vpux_vpu_id_regex{"^VPUX\\.VPU-(\\d+)$"};
            std::smatch vpux_vpu_id_matcher{};

            if (std::regex_match(device_name, vpux_vpu_id_matcher, vpux_vpu_id_regex)) {
                assert(vpux_vpu_id_matcher.size() == 2);

                const std::ssub_match _vpux_vpu_id_matcher = vpux_vpu_id_matcher[1];
                const std::string id_str = _vpux_vpu_id_matcher.str();

                vpu_device_name = "VPU-" + id_str;
            } else {
                throw std::invalid_argument("Device does not match VPUX.VPU-<id> pattern, where <id> is an integer. "
                                            "Check your device name: " +
                                            device_name);
            }
        }
    }
    return std::make_tuple(has_vpu_device_id, vpu_device_name);
}

bool strToBool(const std::string &s) {
    std::istringstream iss(s);
    bool ans;
    iss >> ans;
    if (iss.fail()) {
        // Try bool
        iss.clear();
        iss >> std::boolalpha >> ans;
    }

    if (iss.fail())
        throw std::invalid_argument(s + " cannot be converted to boolean");

    return ans;
}

} // namespace Utils
