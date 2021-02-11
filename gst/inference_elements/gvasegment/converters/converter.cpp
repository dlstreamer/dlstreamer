/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converter.h"
#include "converters/instance_default.h"
#include "converters/pixel_link.h"
#include "converters/semantic_args_plane_max.h"
#include "converters/semantic_default.h"

#include "copy_blob_to_gststruct.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"

using namespace SegmentationPlugin;
using namespace Converters;

void Converter::getLabelByLabelId(GValueArray *labels, int label_id, std::string &out_label) {
    if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
        gchar *class_label = g_value_dup_string(labels->values + label_id);
        out_label = std::string(class_label);
    } else {
        out_label = std::to_string(label_id);
    }
}

constexpr char DEFAULT_CONVERTER_TYPE[] = "semantic_default";
std::string Converter::getConverterType(const GstStructure *s) {
    if (s == nullptr || !gst_structure_has_field(s, "converter"))
        return DEFAULT_CONVERTER_TYPE;
    auto converter_type = gst_structure_get_string(s, "converter");
    if (converter_type == nullptr)
        throw std::runtime_error("model_proc's output_processor has empty converter");
    return converter_type;
}

Converter *Converter::create(const GstStructure *model_proc_info,
                             const std::vector<ModelInputProcessorInfo::Ptr> *inputLayers) {
    auto converter_type = getConverterType(model_proc_info);

    gboolean show_zero_class = false;

    if (model_proc_info) {
        if (gst_structure_has_field(model_proc_info, "show_zero_class"))
            gst_structure_get_boolean(model_proc_info, "show_zero_class", &show_zero_class);
    }

    if (converter_type == "semantic_default") {
        return new SemanticDefaultConverter(show_zero_class);
    } else if (converter_type == "semantic_args_plane_max") {
        return new SemanticArgsPlaneMaxConverter(show_zero_class);
    } else if (converter_type == "instance_default") {
        if (!inputLayers)
            throw std::runtime_error("Instance segmentation model should have InputLayerParams");

        double threshold = 0;
        size_t height = 0;
        size_t width = 0;
        int int_height = 0;
        int int_width = 0;

        for (const auto &item : *inputLayers) {
            if (gst_structure_has_field(item.get()->params, "net_width"))
                gst_structure_get_int(item.get()->params, "net_width", &int_width);
            if (gst_structure_has_field(item.get()->params, "net_height"))
                gst_structure_get_int(item.get()->params, "net_height", &int_height);
        }

        if (int_height <= 0)
            throw std::runtime_error("\"height\" in layer's output_postproc should be > 0");
        if (int_width <= 0)
            throw std::runtime_error("\"width\" in layer's output_postproc should be > 0");

        height = static_cast<size_t>(int_height);
        width = static_cast<size_t>(int_width);

        if (gst_structure_has_field(model_proc_info, "conf_threshold"))
            gst_structure_get_double(model_proc_info, "conf_threshold", &threshold);
        if (threshold < 0)
            throw std::runtime_error("\"conf_threshold\" in layer's output_postproc should be > 0");

        return new InstanceDefaultConverter(height, width, threshold);
    } else if (converter_type == "pixel_link") {
        if (!model_proc_info)
            throw std::runtime_error("model_proc_info shouldn't be a nullptr");

        double cls_threshold = 0.5;
        double link_threshold = 0.5;

        if (gst_structure_has_field(model_proc_info, "cls_threshold"))
            gst_structure_get_double(model_proc_info, "cls_threshold", &cls_threshold);
        else
            GST_WARNING("model proc does not have \"cls_threshold\" parameter. Default value is used: 0.5");

        if (gst_structure_has_field(model_proc_info, "link_threshold"))
            gst_structure_get_double(model_proc_info, "link_threshold", &link_threshold);
        else
            GST_WARNING("model proc does not have \"link_threshold\" parameter. Default value is used: 0.5");

        if (cls_threshold < 0 || cls_threshold > 1)
            throw std::runtime_error("\"cls_threshold\" in layer's output_postproc should be > 0 and <= 1");
        if (link_threshold < 0 || link_threshold > 1)
            throw std::runtime_error("\"link_threshold\" in layer's output_postproc should be > 0 and <= 1");

        return new PixelLinkConverter(cls_threshold, link_threshold, show_zero_class);
    }
    return nullptr;
}

std::vector<uint32_t> Converter::probabilitiesToIndex(const float *data, size_t batches, size_t channels, size_t height,
                                                      size_t width) {
    std::vector<uint32_t> classes(batches * height * width);
    for (size_t b = 0; b < batches; b++) {
        for (size_t h = 0; h < height; h++) {
            for (size_t w = 0; w < width; w++) {
                size_t index = 0;
                float max_value = data[b * channels * height * width + h * width + w];
                for (size_t c = 1; c < channels; c++) {
                    float value = data[b * channels * height * width + c * height * width + h * width + w];
                    if (value > max_value) {
                        max_value = value;
                        index = c;
                    }
                }
                classes[b * height * width + h * width + w] = index;
            }
        }
    }
    return classes;
}

int GetUnbatchedSizeInBytes(std::vector<size_t> &dims, size_t batch_size,
                            InferenceBackend::OutputBlob::Precision precision) {
    if (dims.empty())
        throw std::invalid_argument("Blob has 0 dimensions");

    size_t size = dims[0];
    for (size_t i = 1; i < dims.size(); ++i)
        size = safe_mul(size, dims[i]);

    if (!batch_size)
        throw std::invalid_argument("Batch size must be positive number");

    if (size % batch_size != 0)
        throw std::invalid_argument("The size of Semantic info data doesn't go into batch_size");

    size /= batch_size;

    switch (precision) {
    case InferenceBackend::OutputBlob::Precision::FP32:
        size *= sizeof(float);
        break;
    case InferenceBackend::OutputBlob::Precision::U8:
        break;
    case InferenceBackend::OutputBlob::Precision::I32:
        size *= sizeof(int32_t);
        break;
    case InferenceBackend::OutputBlob::Precision::U32:
        size *= sizeof(uint32_t);
        break;
    default:
        throw std::invalid_argument("Failed to get blob size for blob with " +
                                    std::to_string(static_cast<int>(precision)) + " InferenceEngine::Precision");
    }
    return size;
}

void Converter::copySemanticInfoToGstStructure(const float *data, std::vector<size_t> dims,
                                               const std::string &model_name, const std::string &layer_name,
                                               InferenceBackend::OutputBlob::Precision precision,
                                               InferenceBackend::OutputBlob::Layout layout, size_t batch_size,
                                               size_t batch_index, GstStructure *tensor_structure) {
    if (dims.empty())
        throw std::invalid_argument("Blob has 0 dimensions");

    int size = GetUnbatchedSizeInBytes(dims, batch_size, precision);

    copy_buffer_to_structure(tensor_structure, data + batch_index * size, size);

    gst_structure_set(tensor_structure, "layer_name", G_TYPE_STRING, layer_name.c_str(), "model_name", G_TYPE_STRING,
                      model_name.c_str(), "precision", G_TYPE_INT, static_cast<int>(precision), "layout", G_TYPE_INT,
                      static_cast<int>(layout), NULL);
    dims[0] = 1; // unbatched

    // dims vector to GValueArray
    GValueArray *arr = g_value_array_new(dims.size());
    if (not arr)
        throw std::runtime_error("Failed to create GValueArray with " + std::to_string(dims.size()) + " elements");
    GValue gvalue = G_VALUE_INIT;
    g_value_init(&gvalue, G_TYPE_UINT);
    for (size_t i = 0; i < dims.size(); ++i) {
        g_value_set_uint(&gvalue, safe_convert<unsigned int>(dims[i]));
        g_value_array_append(arr, &gvalue);
    }
    gst_structure_set_array(tensor_structure, "dims", arr);
    g_value_array_free(arr);
}
