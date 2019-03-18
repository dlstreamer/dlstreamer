/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "reid_gallery.h"
#include "tracker.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <stdio.h>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

namespace {
float ComputeReidDistance(const cv::Mat &descr1, const cv::Mat &descr2) {
    float xx = descr1.dot(descr1);
    float yy = descr2.dot(descr2);
    float xy = descr1.dot(descr2);
    float norm = sqrt(xx * yy) + 1e-6;
    return 1.0f - xy / norm;
}

bool file_exists(const std::string &name) {
    std::ifstream f(name.c_str());
    return f.good();
}

inline char separator() {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

std::string folder_name(const std::string &path) {
    size_t found_pos;
    found_pos = path.find_last_of(separator());
    if (found_pos != std::string::npos)
        return path.substr(0, found_pos);
    return std::string(".") + separator();
}

} // namespace

const std::string EmbeddingsGallery::unknown_label = "Unknown";
const int EmbeddingsGallery::unknown_id = TrackedObject::UNKNOWN_LABEL_IDX;

EmbeddingsGallery::EmbeddingsGallery(const std::string &ids_list, double threshold) : reid_threshold(threshold) {
    if (ids_list.empty()) {
        std::cout << "Warning: face reid gallery is empty!"
                  << "\n";
        return;
    }

    cv::FileStorage fs(ids_list, cv::FileStorage::Mode::READ);
    cv::FileNode fn = fs.root();
    int id = 0;
    for (cv::FileNodeIterator fit = fn.begin(); fit != fn.end(); ++fit) {
        cv::FileNode item = *fit;
        std::string label = item.name();
        std::vector<cv::Mat> features;

        if (!item.isMap()) {
            std::cout << "Warning: wrong json format. " << label << " is not a mapping" << std::endl;
            continue;
        }

        cv::FileNode features_item = item["features"];
        if (features_item.isNone()) {
            std::cout << "Warning: no features for label: " << label << std::endl;
            continue;
        }

        for (size_t i = 0; i < features_item.size(); i++) {
            std::string path;
            if (file_exists(features_item[i].string())) {
                path = features_item[i].string();
            } else {
                path = folder_name(ids_list) + separator() + features_item[i].string();
            }
            std::ifstream input(path, std::ifstream::binary);

            if (input) {
                input.seekg(0, input.end);
                int file_size = input.tellg();
                input.seekg(0, input.beg);

                cv::Mat emb(file_size / sizeof(float), 1, CV_32F);
                input.read((char *)emb.data, file_size);
                features.push_back(emb);
                idx_to_id.push_back(id); // this line fixed. Was: (total_images)
            } else {
                std::cout << "Warning: Failed to open feature file: " << path << std::endl;
            }
        }
        identities.emplace_back(features, label, id);
        ++id;
    }
}

std::vector<int> EmbeddingsGallery::GetIDsByEmbeddings(const std::vector<cv::Mat> &embeddings) const {
    if (embeddings.empty() || idx_to_id.empty())
        return std::vector<int>();

    cv::Mat distances(static_cast<int>(embeddings.size()), static_cast<int>(idx_to_id.size()), CV_32F);

    for (int i = 0; i < distances.rows; i++) {
        int k = 0;
        for (size_t j = 0; j < identities.size(); j++) {
            for (const auto &reference_emb : identities[j].embeddings) {
                distances.at<float>(i, k) = ComputeReidDistance(embeddings[i], reference_emb);
                k++;
            }
        }
    }
    KuhnMunkres matcher;
    auto matched_idx = matcher.Solve(distances);
    std::vector<int> output_ids;
    for (auto col_idx : matched_idx) {
        if (distances.at<float>(output_ids.size(), col_idx) > reid_threshold)
            output_ids.push_back(unknown_id);
        else
            output_ids.push_back(idx_to_id[col_idx]);
    }
    return output_ids;
}

std::string EmbeddingsGallery::GetLabelByID(int id) const {
    if (id >= 0 && id < static_cast<int>(identities.size()))
        return identities[id].label;
    else
        return unknown_label;
}

size_t EmbeddingsGallery::size() const {
    return identities.size();
}

std::vector<std::string> EmbeddingsGallery::GetIDToLabelMap() const {
    std::vector<std::string> map;
    map.reserve(identities.size());
    for (const auto &item : identities) {
        map.emplace_back(item.label);
    }
    return map;
}
