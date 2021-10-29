/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <config.h>

#include "types.hpp"

#include "tensor_caps.hpp"

#include <utils.h>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace {
const std::map<std::string, Layout> string_to_layout_map = {
    {"ANY", Layout::ANY}, {"NCHW", Layout::NCHW}, {"NHWC", Layout::NHWC}, {"CHW", Layout::CHW}, {"NC", Layout::NC}};

const std::map<std::string, Precision> string_to_precision_map = {
    {"UNSPECIFIED", Precision::UNSPECIFIED}, {"U8", Precision::U8}, {"FP32", Precision::FP32}};

std::vector<size_t> parse_dims_string(const std::string &dims_string) {
    auto tokens = Utils::splitString(dims_string, ',');
    std::vector<size_t> result;
    result.reserve(tokens.size());
    for (const auto &token : tokens)
        result.emplace_back(std::stoul(token));
    return result;
}
} // namespace

// FIXME: we have 3 similar functions in project, make it common in one place
InferenceBackend::FourCC gst_format_to_four_CC(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG("GST_VIDEO_FORMAT_NV12");
        return InferenceBackend::FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_BGR:
        GST_DEBUG("GST_VIDEO_FORMAT_BGR");
        return InferenceBackend::FourCC::FOURCC_BGR;
    case GST_VIDEO_FORMAT_BGRx:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRx");
        return InferenceBackend::FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRA");
        return InferenceBackend::FourCC::FOURCC_BGRA;
    case GST_VIDEO_FORMAT_RGBA:
        GST_DEBUG("GST_VIDEO_FORMAT_RGBA");
        return InferenceBackend::FourCC::FOURCC_RGBA;
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return InferenceBackend::FourCC::FOURCC_I420;
    default:
        throw std::runtime_error("Unsupported GST Format");
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
}

uint16_t get_channels_count(int video_format) {
    switch (video_format) {
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
        return 4;
    case GST_VIDEO_FORMAT_BGR:
    case GST_VIDEO_FORMAT_I420:
        return 3;
    case GST_VIDEO_FORMAT_NV12:
        return 2;
    default:
        throw std::runtime_error("Unsupported video format");
    }
}

bool layout_to_string(Layout layout, std::string &res) {
    auto found_it = std::find_if(string_to_layout_map.begin(), string_to_layout_map.end(),
                                 [&](const std::pair<std::string, Layout> &pair) { return pair.second == layout; });
    if (found_it == string_to_layout_map.end())
        return false;

    res = found_it->first;
    return true;
}

bool string_to_layout(const std::string &str, Layout &res) {
    auto found_it = string_to_layout_map.find(str);
    if (found_it == string_to_layout_map.end())
        return false;

    res = found_it->second;
    return true;
}

bool precision_to_string(Precision precision, std::string &res) {
    auto found_it =
        std::find_if(string_to_precision_map.begin(), string_to_precision_map.end(),
                     [&](const std::pair<std::string, Precision> &pair) { return pair.second == precision; });
    if (found_it == string_to_precision_map.end())
        return false;

    res = found_it->first;
    return true;
}

bool string_to_precision(const std::string &str, Precision &res) {
    auto found_it = string_to_precision_map.find(str);
    if (found_it == string_to_precision_map.end())
        return false;

    res = found_it->second;
    return true;
}

bool string_to_format(const std::string &str, int &res) {
    // TODO: should be not only video
    res = gst_format_to_four_CC(gst_video_format_from_string(str.c_str()));
    return res != 0;
}

TensorCaps TensorCaps::FromCaps(const GstCaps *caps) {
    if (!caps)
        throw std::invalid_argument("Invalid capabilities pointer");
    if (gst_caps_get_size(caps) != 1)
        throw std::invalid_argument("Capabilities should have one structure");

    auto memory_type = get_memory_type_from_caps(caps);
    auto structure = gst_caps_get_structure(caps, 0);
    return FromStructure(structure, memory_type);
}

TensorCaps TensorCaps::FromStructure(const GstStructure *structure, InferenceBackend::MemoryType memory_type) {
    if (!structure)
        throw std::invalid_argument("Invalid gst structure pointer");

    if (!gst_structure_has_name(structure, "application/tensor"))
        throw std::invalid_argument("Capabilities are not 'application/tensor' type");

    // TODO: make layer_name field required?
    auto layer_name_str = gst_structure_get_string(structure, "layer_name");
    if (!layer_name_str)
        GST_WARNING("Couldn't get 'layer_name' field from structure");

    auto precision_str = gst_structure_get_string(structure, "precision");
    auto layout_str = gst_structure_get_string(structure, "layout");
    auto dims_str = gst_structure_get_string(structure, "dims");

    Precision precision = Precision::UNSPECIFIED;
    Layout layout = Layout::ANY;
    if (!precision_str || !layout_str || !dims_str)
        throw std::invalid_argument("Invalid capabilities structure format");

    if (!string_to_precision(precision_str, precision) || !string_to_layout(layout_str, layout)) {
        throw std::invalid_argument("Invalid precision or layout values");
    }

    auto dims = parse_dims_string(dims_str);
    return TensorCaps(memory_type, precision, layout, dims, layer_name_str ? layer_name_str : std::string());
}

GstCaps *TensorCaps::ToCaps(const TensorCaps &tensor_caps) {
    auto result = tensor_caps.GetMemoryType() == InferenceBackend::MemoryType::SYSTEM
                      ? gst_caps_from_string(GVA_TENSOR_CAPS)
                      : gst_caps_from_string(GVA_VAAPI_TENSOR_CAPS);
    auto structure = gst_caps_get_structure(result, 0);
    if (!ToStructure(tensor_caps, structure)) {
        GST_WARNING("Failed to set TensorCaps to structure");
        gst_caps_unref(result);
        return nullptr;
    }

    return result;
}

bool TensorCaps::ToStructure(const TensorCaps &tensor_caps, GstStructure *structure) {
    if (!structure)
        throw std::invalid_argument("Invalid gst structure pointer");

    gst_structure_set_name(structure, "application/tensor");

    std::string precision_str;
    std::string layout_str;
    if (!precision_to_string(tensor_caps._precision, precision_str) ||
        !layout_to_string(tensor_caps._layout, layout_str))
        return false;
    auto dims_str = Utils::join(tensor_caps._dims.begin(), tensor_caps._dims.end());

    gst_structure_set(structure, "precision", G_TYPE_STRING, precision_str.c_str(), "layout", G_TYPE_STRING,
                      layout_str.c_str(), "dims", G_TYPE_STRING, dims_str.c_str(), "layer_name", G_TYPE_STRING,
                      tensor_caps._layer_name.c_str(), nullptr);
    return true;
}

TensorCaps::TensorCaps(InferenceBackend::MemoryType memory_type, Precision precision, Layout layout,
                       const std::vector<size_t> &dims, const std::string &layer_name)
    : _memory_type(memory_type), _precision(precision), _layout(layout), _dims(dims), _layer_name(layer_name) {
    auto expected_dims_size = 4u;
    switch (_layout) {
    case Layout::NC:
        expected_dims_size = 2;
        break;
    case Layout::CHW:
        expected_dims_size = 3;
        break;
    default:
        break;
    }

    if (_dims.size() != expected_dims_size) {
        throw std::runtime_error("Invalid dims size for specified layout");
    }
}

InferenceBackend::MemoryType TensorCaps::GetMemoryType() const {
    return _memory_type;
}

Precision TensorCaps::GetPrecision() const {
    return _precision;
}

Layout TensorCaps::GetLayout() const {
    return _layout;
}

std::vector<size_t> TensorCaps::GetDims() const {
    return _dims;
}

bool TensorCaps::HasBatchSize() const {
    switch (_layout) {
    case Layout::NC:
    case Layout::NCHW:
    case Layout::NHWC:
        return true;
    default:
        return false;
    }
}

size_t TensorCaps::GetBatchSize() const {
    if (!HasBatchSize())
        return 0;
    return _dims.front();
}

size_t TensorCaps::GetChannels() const {
    switch (_layout) {
    case Layout::NHWC:
        return _dims.back();
    case Layout::NCHW:
    case Layout::NC:
        return _dims.at(1);
    case Layout::CHW:
        return _dims.front();
    default:
        return 0;
    }
}

size_t TensorCaps::GetWidth() const {
    switch (_layout) {
    case Layout::NCHW:
    case Layout::CHW:
        return _dims.back();
    case Layout::NHWC:
        return _dims.rbegin()[1];
    default:
        return 0;
    }
}

size_t TensorCaps::GetHeight() const {
    switch (_layout) {
    case Layout::NCHW:
        return _dims.rbegin()[1];
    case Layout::NHWC:
    case Layout::CHW:
        return _dims[1];
    default:
        return 0;
    }
}

std::string TensorCaps::GetLayerName() const {
    return _layer_name;
}

TensorCapsArray TensorCapsArray::FromCaps(const GstCaps *caps) {
    if (!caps)
        throw std::invalid_argument("Invalid capabilities pointer");

    if (gst_caps_get_size(caps) != 1)
        throw std::invalid_argument("Capabilities should have one structure");

    auto structure = gst_caps_get_structure(caps, 0);
    TensorCapsArray result;
    if (gst_structure_has_name(structure, "application/tensors")) {
        auto mem_type = get_memory_type_from_caps(caps);
        if (!gst_structure_has_field(structure, "descs"))
            throw std::invalid_argument("Invalid tensor caps format");

        GValueArray *desc_arr = nullptr;
        if (!gst_structure_get_array(structure, "descs", &desc_arr))
            throw std::runtime_error("Failed to get tensor descriptions array");

        result._tensor_descs.reserve(desc_arr->n_values);
        for (size_t i = 0; i < desc_arr->n_values; ++i) {
            auto desc_struct = gst_value_get_structure(g_value_array_get_nth(desc_arr, i));
            result._tensor_descs.emplace_back(TensorCaps::FromStructure(desc_struct, mem_type));
        }
        g_value_array_free(desc_arr);
    } else if (gst_structure_has_name(structure, "application/tensor")) {
        result._tensor_descs.emplace_back(TensorCaps::FromCaps(caps));
    } else
        throw std::invalid_argument("Expected caps with 'application/tensors' or 'application/tensor' names");

    return result;
}

GstCaps *TensorCapsArray::ToCaps(const TensorCapsArray &tensor_caps_array) {
    auto result = tensor_caps_array.GetMemoryType() == InferenceBackend::MemoryType::SYSTEM
                      ? gst_caps_from_string(GVA_TENSORS_CAPS)
                      : gst_caps_from_string(GVA_VAAPI_TENSORS_CAPS);
    GValue descs = G_VALUE_INIT;
    gst_value_array_init(&descs, 0);
    for (const auto &desc : tensor_caps_array._tensor_descs) {
        auto gst_struct_desc = gst_structure_new_empty("application/tensor");
        if (!TensorCaps::ToStructure(desc, gst_struct_desc))
            throw std::runtime_error("Failed to set TensorCaps to structure");

        GValue gval_desc = G_VALUE_INIT;
        g_value_init(&gval_desc, GST_TYPE_STRUCTURE);
        gst_value_set_structure(&gval_desc, gst_struct_desc);
        gst_value_array_append_value(&descs, &gval_desc);
    }

    gst_caps_set_value(result, "descs", &descs);
    return result;
}

TensorCapsArray::TensorCapsArray(const std::vector<TensorCaps> &tensor_descs) : _tensor_descs(tensor_descs) {
}

size_t TensorCapsArray::GetTensorNum() const {
    return _tensor_descs.size();
}

const TensorCaps &TensorCapsArray::GetTensorDesc(size_t index) const {
    return _tensor_descs.at(index);
}

InferenceBackend::MemoryType TensorCapsArray::GetMemoryType() const {
    if (_tensor_descs.empty())
        return InferenceBackend::MemoryType::ANY;

    return _tensor_descs.front().GetMemoryType();
}
