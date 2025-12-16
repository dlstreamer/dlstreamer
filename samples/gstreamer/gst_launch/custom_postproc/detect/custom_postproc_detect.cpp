/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <algorithm>
#include <gst/analytics/analytics.h>
#include <gst/gst.h>
#include <stdexcept>
#include <string>
#include <vector>

const int YOLOV11_OFFSET_X = 0;
const int YOLOV11_OFFSET_Y = 1;
const int YOLOV11_OFFSET_W = 2;
const int YOLOV11_OFFSET_H = 3;
const int YOLOV11_OFFSET_CS = 4;

extern "C" void Convert(GstTensorMeta *outputTensors, const GstStructure *network, const GstStructure *params,
                        GstAnalyticsRelationMeta *relationMeta) {
    const GstTensor *tensor = gst_tensor_meta_get(outputTensors, outputTensors->num_tensors - 1);
    size_t dims_size;
    size_t *dims = gst_tensor_get_dims(gst_tensor_copy(tensor), &dims_size);

    if (dims_size < 2)
        throw std::invalid_argument("Invalid tensor dimensions.");

    size_t object_size = dims[dims_size - 2];
    size_t max_proposal_count = dims[dims_size - 1];

    size_t input_width = 0, input_height = 0;
    gst_structure_get_uint64(network, "image_width", &input_width);
    gst_structure_get_uint64(network, "image_height", &input_height);

    double confidence_threshold = 0.5;
    gst_structure_get_double(params, "confidence_threshold", &confidence_threshold);

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

    std::vector<float> transposed_data(object_size * max_proposal_count);
    for (size_t i = 0; i < object_size; ++i) {
        for (size_t j = 0; j < max_proposal_count; ++j) {
            transposed_data[j * object_size + i] = data[i * max_proposal_count + j];
        }
    }

    for (size_t i = 0; i < max_proposal_count; ++i) {
        float *output_data = transposed_data.data() + i * object_size;
        float *classes_scores = output_data + YOLOV11_OFFSET_CS;

        double maxClassScore = classes_scores[0];
        int class_id = 0;
        for (size_t j = 1; j < (object_size - YOLOV11_OFFSET_CS); ++j) {
            if (classes_scores[j] > maxClassScore) {
                maxClassScore = classes_scores[j];
                class_id = j;
            }
        }

        if (maxClassScore > confidence_threshold) {
            float x_center = output_data[YOLOV11_OFFSET_X];
            float y_center = output_data[YOLOV11_OFFSET_Y];
            float width = output_data[YOLOV11_OFFSET_W];
            float height = output_data[YOLOV11_OFFSET_H];

            int x = static_cast<int>(x_center - width / 2);
            int y = static_cast<int>(y_center - height / 2);
            int w = static_cast<int>(width);
            int h = static_cast<int>(height);

            GQuark label = g_quark_from_string(labels[class_id].c_str());

            GstAnalyticsODMtd od_mtd;
            if (!gst_analytics_relation_meta_add_od_mtd(relationMeta, label, x, y, w, h, (gfloat)maxClassScore,
                                                        &od_mtd))
                throw std::runtime_error("Failed to add OD metadata.");
        }
    }
}
