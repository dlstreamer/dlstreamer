/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_converters.h"
#include "gva_roi_meta.h"
#include <iomanip>
#include <sstream>
#include <string.h>
#include <string>

static void find_max_element_index(const float *array, int len, int *index, float *value) {
    *index = 0;
    *value = array[0];
    for (int i = 1; i < len; i++) {
        if (array[i] > *value) {
            *index = i;
            *value = array[i];
        }
    }
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

bool Attributes2Text(GstStructure *s) {
    const gchar *_method = gst_structure_get_string(s, "method");
    std::string method = _method ? _method : "";
    bool bMax = method == "max";
    bool bCompound = method == "compound";
    bool bIndex = method == "index";

    if (!bMax && !bCompound && !bIndex)
        bMax = true;

    gsize nbytes = 0;
    const float *data = (const float *)gva_get_tensor_data(s, &nbytes);
    if (!data)
        return false;
    GValueArray *labels = nullptr;
    if (!gst_structure_get_array(s, "labels", &labels))
        return false;
    if (!bIndex) {
        if (labels->n_values != (bCompound ? 2 : 1) * nbytes / sizeof(float)) {
            g_value_array_free(labels);
            return false;
        }
    }
    if (bMax) {
        int index;
        float confidence;
        find_max_element_index(data, labels->n_values, &index, &confidence);
        if (data[index] > 0) {
            const gchar *label = g_value_get_string(labels->values + index);
            gst_structure_set(s, "label", G_TYPE_STRING, label, "label_id", G_TYPE_INT, (gint)index, "confidence",
                              G_TYPE_DOUBLE, (gdouble)confidence, NULL);
        }
    } else if (bCompound) {
        std::string string;
        double threshold = 0.5;
        double confidence = 0;
        gst_structure_get_double(s, "threshold", &threshold);
        for (guint j = 0; j < (labels->n_values) / 2; j++) {
            const gchar *label = NULL;
            if (data[j] >= threshold) {
                label = g_value_get_string(labels->values + j * 2);
            } else if (data[j] > 0) {
                label = g_value_get_string(labels->values + j * 2 + 1);
            }
            if (label)
                string += label;
            if (data[j] >= confidence)
                confidence = data[j];
        }
        gst_structure_set(s, "label", G_TYPE_STRING, string.data(), "confidence", G_TYPE_DOUBLE, (gdouble)confidence,
                          NULL);
    } else if (bIndex) {
        std::string string;
        int max_value = 0;
        for (guint j = 0; j < nbytes / sizeof(float); j++) {
            int value = (int)data[j];
            if (value < 0 || (guint)value >= labels->n_values)
                break;
            if (value > max_value)
                max_value = value;
            string += g_value_get_string(labels->values + value);
        }
        if (max_value) {
            gst_structure_set(s, "label", G_TYPE_STRING, string.data(), NULL);
        }
    } else {
        double threshold = 0.5;
        double confidence = 0;
        gst_structure_get_double(s, "threshold", &threshold);
        for (guint j = 0; j < labels->n_values; j++) {
            if (data[j] >= threshold) {
                const gchar *label = g_value_get_string(labels->values + j);
                gst_structure_set(s, "label", G_TYPE_STRING, label, "confidence", G_TYPE_DOUBLE, (gdouble)confidence,
                                  NULL);
            }
            if (data[j] >= confidence)
                confidence = data[j];
        }
    }

    if (labels)
        g_value_array_free(labels);
    return true;
}

G_GNUC_END_IGNORE_DEPRECATIONS

bool Tensor2Text(GstStructure *s) {
    gsize nbytes = 0;
    const float *data = (const float *)gva_get_tensor_data(s, &nbytes);
    if (!data)
        return false;
    gdouble scale = 1.0;
    gst_structure_get_double(s, "tensor2text_scale", &scale);
    gint precision = 2;
    gst_structure_get_int(s, "tensor2text_precision", &precision);
    std::stringstream stream;
    stream << std::fixed << std::setprecision(precision);
    for (size_t i = 0; i < nbytes / sizeof(float); i++) {
        if (i)
            stream << ", ";
        stream << data[i] * scale;
    }
    gst_structure_set(s, "label", G_TYPE_STRING, stream.str().data(), NULL);
    return true;
}

bool ConvertMeta(GstStructure *s) {
    const gchar *converter = gst_structure_get_string(s, "converter");
    if (!converter)
        return false;
    if (!strcmp(converter, "attributes")) {
        return Attributes2Text(s);
    }
    if (!strcmp(converter, "tensor2text")) {
        return Tensor2Text(s);
    }
    return false;
}
