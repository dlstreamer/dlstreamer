/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Reidentification gallery implementation based on smart classroom demo
 * See https://github.com/opencv/open_model_zoo/tree/2018/demos/smart_classroom_demo
 * Differences:
 * Store features in separate feature file instead of embedding into images
 * Adapted code style to match with Video Analytics GStreamer* plugins project
 * Fixed warnings
 ******************************************************************************/

#include "reid_gallery.h"
#include "gallery_schema.h"

#include <fstream>
#include <iostream>
#include <limits>
#include <stdio.h>
#include <string>
#include <vector>

#include "inference_backend/safe_arithmetic.h"
#include <json-schema.hpp>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

using json = nlohmann::json;
using nlohmann::json_schema::json_validator;

namespace {
float ComputeReidDistance(const cv::Mat &descr1, const cv::Mat &descr2) {
    float xx = safe_convert<float>(descr1.dot(descr1));
    float yy = safe_convert<float>(descr2.dot(descr2));
    float xy = safe_convert<float>(descr1.dot(descr2));
    float norm = sqrt(xx * yy) + 1e-6f;
    float cosine_similarity = xy / norm;
    return cosine_similarity;
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
const int EmbeddingsGallery::unknown_id = -1;

EmbeddingsGallery::EmbeddingsGallery(GstBaseTransform *base_transform, std::string ids_list, double threshold)
    : reid_threshold(threshold) {
    std::ifstream input_file(ids_list);
    if (!input_file.is_open()) {
        GST_ELEMENT_ERROR(base_transform, RESOURCE, NOT_FOUND, ("gallery file failed to open"),
                          ("Cannot open gallery file: %s.", ids_list.c_str()));
        return;
    }
    json gallery_json;
    if (!json::accept(input_file)) {
        GST_ELEMENT_ERROR(base_transform, RESOURCE, SETTINGS, ("gallery file is not json"),
                          ("gallery file %s is not proper json", ids_list.c_str()));
        return;
    }
    input_file.seekg(0, input_file.beg);
    input_file >> gallery_json;
    input_file.close();
    json_validator validator;
    try {
        validator.set_root_schema(GALLERY_SCHEMA);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_transform, RESOURCE, SETTINGS, ("gallery json schema failed to load."),
                          ("gallery json schema failed to load. Error: %s", e.what()));
        return;
    }
    try {
        validator.validate(gallery_json);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(base_transform, RESOURCE, SETTINGS, ("gallery json validation failed"),
                          ("gallery json validation failed for file %s Error: %s", ids_list.c_str(), e.what()));
        return;
    }

    int id = 0;
    for (json::iterator jit = gallery_json.begin(); jit != gallery_json.end(); jit++) {
        const int tensor_mat_rows = 256;
        const int tensor_mat_num_bytes = 1024;
        json item = *jit;
        std::string label = item["name"];
        std::vector<cv::Mat> features;
        json features_array = item["features"];
        for (guint i = 0; i < features_array.size(); i++) {
            std::string path;
            path = features_array[i].dump();
            path.erase(path.begin());
            path.erase(path.end() - 1);
            if (!file_exists(path)) {
                path = folder_name(ids_list) + separator() + path;
            }
            std::ifstream input(path, std::ifstream::binary);
            if (input) {
                cv::Mat emb(tensor_mat_rows, 1, CV_32F);
                if (emb.total() != tensor_mat_rows) {
                    GST_ELEMENT_ERROR(base_transform, RESOURCE, SETTINGS, ("Tensor file is wrong size"),
                                      ("Tensor file %s has invalid data", path.c_str()));
                    return;
                }
                input.read((char *)emb.data, tensor_mat_num_bytes);

                for (int i = 0; i < emb.rows; i++) {
                    if (emb.at<float>(i, 0) != emb.at<float>(i, 0)) {
                        GST_ELEMENT_ERROR(base_transform, RESOURCE, SETTINGS, ("Tensor file has NaN"),
                                          ("Tensor file %s has invalid data", path.c_str()));
                        return;
                    }
                }

                features.push_back(emb);
                idx_to_id.push_back(id);
            } else {
                GST_ERROR("Failed to open feature file: %s", path.c_str());
            }
        }
        identities.emplace_back(features, label, id);
        ++id;
    }
}

std::vector<std::pair<int, float>> EmbeddingsGallery::GetIDsByEmbeddings(const std::vector<cv::Mat> &embeddings) const {
    if (embeddings.empty() || idx_to_id.empty())
        return std::vector<std::pair<int, float>>();

    cv::Mat distances(safe_convert<int>(embeddings.size()), safe_convert<size_t, int>(idx_to_id.size()), CV_32F);

    for (size_t i = 0; i < safe_convert<size_t>(distances.rows); i++) {
        size_t k = 0;
        for (size_t j = 0; j < identities.size(); j++) {
            for (const auto &reference_emb : identities[j].embeddings) {
                distances.at<float>(i, k) = ComputeReidDistance(embeddings[i], reference_emb);
                k++;
            }
        }
    }

    std::vector<std::pair<int, float>> output_ids;
    for (int row = 0; row < distances.rows; ++row) {
        float similarity = distances.at<float>(row, 0);
        size_t similarity_id = 0;
        for (int col = 1; col < distances.cols; ++col) {
            if (similarity < distances.at<float>(row, col)) {
                similarity = distances.at<float>(row, col);
                similarity_id = col;
            }
        }
        if (similarity < reid_threshold) {
            output_ids.push_back(std::make_pair(unknown_id, similarity));
        } else {

            output_ids.push_back(std::make_pair(idx_to_id[similarity_id], similarity));
        }
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
