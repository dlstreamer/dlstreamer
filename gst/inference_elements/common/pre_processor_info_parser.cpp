/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "pre_processor_info_parser.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<double> GValueArrayToVector(GValueArray *arr) {

    std::vector<double> vec;
    vec.reserve(arr->n_values);
    for (guint i = 0; i < arr->n_values; ++i) {
        vec.push_back(g_value_get_double(g_value_array_get_nth(arr, i)));
    }
    return vec;
}

void deleteGValueArr(GValueArray *arr) {
    if (arr) {
        g_value_array_free(arr);
        arr = nullptr;
    }
}
} // namespace

PreProcParamsParser::PreProcParamsParser(const GstStructure *params) : params(params) {
}

InferenceBackend::InputImageLayerDesc::Ptr PreProcParamsParser::parse() const {
    if (!params or !gst_structure_n_fields(params)) {
        return nullptr;
    }

    const auto resize = getResize();
    const auto crop = getCrop();

    const auto color_space = getColorSpace();
    const auto range_norm = getRangeNormalization();
    const auto distrib_norm = getDistribNormalization();

    return std::make_shared<InferenceBackend::InputImageLayerDesc>(
        InferenceBackend::InputImageLayerDesc(resize, crop, color_space, range_norm, distrib_norm));
}

PreProcResize PreProcParamsParser::getResize() const {
    PreProcResize resize_val = PreProcResize::NO;

    if (gst_structure_has_field(params, "resize")) {
        const gchar *_resize_type = gst_structure_get_string(params, "resize");
        if (!_resize_type) {
            throw std::runtime_error("\"resize\" string was broken.");
        }
        std::string resize_type(_resize_type);
        if (resize_type == "aspect-ratio") {
            resize_val = PreProcResize::ASPECT_RATIO;
        } else if (resize_type == "no-aspect-ratio") {
            resize_val = PreProcResize::NO_ASPECT_RATIO;
        } else {
            throw std::runtime_error(std::string("Invalid type of resize: ") + resize_type);
        }
    }

    return resize_val;
}

PreProcCrop PreProcParamsParser::getCrop() const {
    PreProcCrop crop_val = PreProcCrop::NO;

    if (gst_structure_has_field(params, "crop")) {
        const gchar *_crop_type = gst_structure_get_string(params, "crop");
        if (!_crop_type) {
            throw std::runtime_error("\"crop\" string was broken.");
        }
        std::string crop_type(_crop_type);
        if (crop_type == "central") {
            crop_val = PreProcCrop::CENTRAL;
        } else if (crop_type == "top_left") {
            crop_val = PreProcCrop::TOP_LEFT;
        } else if (crop_type == "top_right") {
            crop_val = PreProcCrop::TOP_RIGHT;
        } else if (crop_type == "bottom_left") {
            crop_val = PreProcCrop::BOTTOM_LEFT;
        } else if (crop_type == "bottom_right") {
            crop_val = PreProcCrop::BOTTOM_RIGHT;
        } else {
            throw std::runtime_error(std::string("Invalid type of crop: ") + crop_type);
        }
    }
    return crop_val;
}

PreProcColorSpace PreProcParamsParser::getColorSpace() const {
    PreProcColorSpace color_space_val = PreProcColorSpace::NO;
    if (gst_structure_has_field(params, "color_space")) {
        const gchar *_color_space_type = gst_structure_get_string(params, "color_space");
        if (!_color_space_type) {
            throw std::runtime_error("\"color_space\" string was broken.");
        }
        std::string color_space_type(_color_space_type);
        if (color_space_type == "RGB") {
            color_space_val = PreProcColorSpace::RGB;
        } else if (color_space_type == "BGR") {
            color_space_val = PreProcColorSpace::BGR;
        } else if (color_space_type == "YUV") {
            color_space_val = PreProcColorSpace::YUV;
        } else if (color_space_type == "GRAYSCALE") {
            color_space_val = PreProcColorSpace::GRAYSCALE;
        } else {
            throw std::runtime_error(std::string("Invalid target color format: ") + color_space_type);
        }
    }
    return color_space_val;
}

PreProcRangeNormalization PreProcParamsParser::getRangeNormalization() const {
    GValueArray *arr = nullptr;
    try {
        if (gst_structure_has_field(params, "range")) {
            gst_structure_get_array(const_cast<GstStructure *>(params), "range", &arr);
            if (!arr or arr->n_values != 2)
                throw std::runtime_error("Invalid \"range\" array in model-proc file. It should only contain two "
                                         "values (minimum and maximum)");

            auto range = GValueArrayToVector(arr);
            deleteGValueArr(arr);
            return PreProcRangeNormalization(range[0], range[1]);
        }
    } catch (const std::exception &e) {
        deleteGValueArr(arr);
        std::throw_with_nested(e);
    }
    return PreProcRangeNormalization();
}

PreProcDistribNormalization PreProcParamsParser::getDistribNormalization() const {
    GValueArray *arr = nullptr;
    try {
        if (gst_structure_has_field(params, "mean") and gst_structure_has_field(params, "std")) {
            std::vector<double> std, mean;

            gst_structure_get_array(const_cast<GstStructure *>(params), "mean", &arr);
            if (!arr or !arr->n_values)
                throw std::runtime_error("\"mean\" array is null.");

            mean = GValueArrayToVector(arr);
            deleteGValueArr(arr);

            gst_structure_get_array(const_cast<GstStructure *>(params), "std", &arr);
            if (!arr or !arr->n_values)
                throw std::runtime_error("\"std\" array is null.");

            std = GValueArrayToVector(arr);
            g_value_array_free(arr);
            arr = nullptr;

            return PreProcDistribNormalization(mean, std);
        }
    } catch (const std::exception &e) {
        deleteGValueArr(arr);
        std::throw_with_nested(e);
    }
    return PreProcDistribNormalization();
}
