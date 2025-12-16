/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <filesystem>
#include <fstream>
#include <vector>

// Reads labels from file into vector
inline std::vector<std::string> load_labels_file(const std::filesystem::path &file_path) {
    if (!std::filesystem::exists(file_path))
        throw std::invalid_argument("Labels file '" + file_path.generic_string() + "' does not exist");

    std::vector<std::string> res;
    std::ifstream fstream(file_path);
    for (std::string line; std::getline(fstream, line);) {
        res.push_back(line);
    }

    return res;
}
