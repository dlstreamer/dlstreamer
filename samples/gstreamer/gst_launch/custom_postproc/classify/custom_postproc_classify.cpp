/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/analytics/analytics.h>
#include <gst/gst.h>
#include <stdexcept>
#include <string>
#include <vector>

#include <algorithm>
#include <cmath>

extern "C" void Convert(GstTensorMeta *outputTensors, const GstStructure *network, const GstStructure *params,
                        GstAnalyticsRelationMeta *relationMeta) {
    const GstTensor *tensor = gst_tensor_meta_get(outputTensors, outputTensors->num_tensors - 1);
    size_t dims_size;
    size_t *dims = gst_tensor_get_dims(gst_tensor_copy(tensor), &dims_size);

    if (dims_size < 2)
        throw std::invalid_argument("Invalid tensor dimensions.");

    size_t size = dims[dims_size - 1];

    size_t input_width = 0, input_height = 0;
    gst_structure_get_uint64(network, "image_width", &input_width);
    gst_structure_get_uint64(network, "image_height", &input_height);

    std::vector<std::string> labels;
    const GValue *labels_value = gst_structure_get_value(network, "labels");
    if (labels_value && G_VALUE_HOLDS(labels_value, GST_TYPE_ARRAY)) {
        int n_labels = gst_value_array_get_size(labels_value);
        for (int i = 0; i < n_labels; ++i) {
            const GValue *item = gst_value_array_get_value(labels_value, i);
            if (G_VALUE_HOLDS_STRING(item))
                labels.push_back(g_value_get_string(item));
        }
    }

    float *data = nullptr;
    GstMapInfo map;
    if (gst_buffer_map(tensor->data, &map, GST_MAP_READ)) {
        data = reinterpret_cast<float *>(map.data);
        gst_buffer_unmap(tensor->data, &map);
    } else {
        throw std::runtime_error("Failed to map GstBuffer.");
    }

    auto max_confidence = std::max_element(data, data + size);
    std::vector<float> sftm_arr(size);
    float sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sftm_arr[i] = std::exp(data[i] - *max_confidence);
        sum += sftm_arr.at(i);
    }
    if (sum > 0) {
        for (size_t i = 0; i < size; ++i) {
            sftm_arr[i] /= sum;
        }
    }
    auto max_elem = std::max_element(sftm_arr.begin(), sftm_arr.end());
    auto index = std::distance(sftm_arr.begin(), max_elem);
    auto label = labels.at(index);

    GQuark label_gquark = g_quark_from_string(label.c_str());

    float max_elem_value = static_cast<float>(*max_elem);

    GstAnalyticsClsMtd cls_mtd;
    if (!gst_analytics_relation_meta_add_one_cls_mtd(relationMeta, max_elem_value, label_gquark, &cls_mtd))
        throw std::runtime_error("Failed to add class metadata.");
}
