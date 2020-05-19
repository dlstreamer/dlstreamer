/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_utils.h"
#include <fstream>
#include <sstream>
#include <string>

std::string CreateNestedErrorMsg(const std::exception &e, int level) {
    static std::string msg = "\n";
    msg += std::string(level, ' ') + e.what() + "\n";
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        CreateNestedErrorMsg(e, level + 1);
    }
    return msg;
}

std::vector<std::string> SplitString(const std::string &input, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(input);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

bool file_exists(const std::string &path) {
    return std::ifstream(path).good();
}

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id) {
    GstStructure *object_id = gst_video_region_of_interest_meta_get_param(meta, "object_id");
    return object_id && gst_structure_get_int(object_id, "id", id);
}

void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id) {
    GstStructure *object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
    gst_video_region_of_interest_meta_add_param(meta, object_id);
}