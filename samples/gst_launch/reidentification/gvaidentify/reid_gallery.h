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

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <map>
#include <string>
#include <vector>

#include <opencv2/core/core.hpp>

#define GALLERY_MAGIC_VALUE 0x47166923

struct GalleryObject {
    std::vector<cv::Mat> embeddings;
    std::string label;
    int id;

    GalleryObject(const std::vector<cv::Mat> &embeddings, const std::string &label, int id)
        : embeddings(embeddings), label(label), id(id) {
    }
};

class EmbeddingsGallery {
  public:
    static const std::string unknown_label;
    static const int unknown_id;
    EmbeddingsGallery(GstBaseTransform *base_transform, std::string ids_list, double threshold);
    size_t size() const;
    std::vector<int> GetIDsByEmbeddings(const std::vector<cv::Mat> &embeddings) const;
    std::string GetLabelByID(int id) const;
    std::vector<std::string> GetIDToLabelMap() const;

  private:
    std::vector<int> idx_to_id;
    double reid_threshold;
    std::vector<GalleryObject> identities;
};
