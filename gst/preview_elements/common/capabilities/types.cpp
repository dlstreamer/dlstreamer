/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <config.h>

#include "types.hpp"

#include "tensor_caps.hpp"

#include <safe_arithmetic.hpp>
#include <utils.h>

#include <algorithm>
#include <map>
#include <stdexcept>

namespace {

#define STR_LAYOUT_TUPLE(layout)                                                                                       \
    { #layout, Layout::layout }

const std::map<std::string, Layout> string_to_layout_map = {
    STR_LAYOUT_TUPLE(ANY),     STR_LAYOUT_TUPLE(NCHW),   STR_LAYOUT_TUPLE(NHWC),  STR_LAYOUT_TUPLE(NCDHW),
    STR_LAYOUT_TUPLE(NDHWC),   STR_LAYOUT_TUPLE(OIHW),   STR_LAYOUT_TUPLE(GOIHW), STR_LAYOUT_TUPLE(OIDHW),
    STR_LAYOUT_TUPLE(GOIDHW),  STR_LAYOUT_TUPLE(SCALAR), STR_LAYOUT_TUPLE(C),     STR_LAYOUT_TUPLE(CHW),
    STR_LAYOUT_TUPLE(HWC),     STR_LAYOUT_TUPLE(HW),     STR_LAYOUT_TUPLE(NC),    STR_LAYOUT_TUPLE(CN),
    STR_LAYOUT_TUPLE(BLOCKED),
};

#undef STR_LAYOUT_TUPLE

std::vector<size_t> parse_dims_string(const std::string &dims_string) {
    auto tokens = Utils::splitString(dims_string, ':');
    std::vector<size_t> result;
    result.reserve(tokens.size());
    for (const auto &token : tokens)
        result.emplace_back(std::stoul(token));
    std::reverse(result.begin(), result.end()); // reverse order
    return result;
}

std::string dims_to_string(std::vector<size_t> dims) {
    std::reverse(dims.begin(), dims.end()); // reverse order
    return Utils::join(dims.begin(), dims.end(), ':');
}

Layout layout_from_dims(const std::vector<size_t> &dims) {
    const auto dims_size = dims.size();
    if (dims_size == 0 || dims_size > 4)
        throw std::invalid_argument("Can't deduct tensor layout: dimesions size is invalid: " +
                                    std::to_string(dims_size));

    if (dims_size == 1)
        return Layout::C;

    if (dims_size == 2)
        return Layout::NC;

    const auto n_offset = dims_size - 3;
    const auto is_hwc = dims[n_offset] > 4 && dims[n_offset + 1] > 4 && dims[n_offset + 2] <= 4;

    // TODO: is it OK to fallback to (N)CHW layout?
    if (dims.size() == 3) {
        return is_hwc ? Layout::HWC : Layout::CHW;
    }

    if (dims.size() == 4) {
        return is_hwc ? Layout::NHWC : Layout::NCHW;
    }

    throw std::runtime_error("Can't deduct tensor layout from dimensions: " + dims_to_string(dims));
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
                                 [layout](const std::pair<std::string, Layout> &p) { return p.second == layout; });
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
    switch (precision) {
    case Precision::U8:
        res = "uint8";
        break;
    case Precision::FP32:
        res = "float32";
        break;
    case Precision::I32:
        res = "int32";
        break;
    default:
        return false;
    };
    return true;
}

bool string_to_precision(const std::string &str, Precision &res) {
    if (str == "uint8")
        res = Precision::U8;
    else if (str == "float32")
        res = Precision::FP32;
    else if (str == "int32")
        res = Precision::I32;
    else
        return false;
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

    auto precision_str = gst_structure_get_string(structure, "types");
    auto dims_str = gst_structure_get_string(structure, "dimensions");

    if (!precision_str || !dims_str)
        throw std::invalid_argument("Invalid capabilities structure format");

    Precision precision = Precision::UNSPECIFIED;
    if (!string_to_precision(precision_str, precision)) {
        throw std::invalid_argument("Invalid precision");
    }
    auto dims = parse_dims_string(dims_str);
    Layout layout = layout_from_dims(dims);
    return TensorCaps(memory_type, precision, layout, dims);
}

GstCaps *TensorCaps::ToCaps(const TensorCaps &tensor_caps) {
    auto result = tensor_caps.GetMemoryType() == InferenceBackend::MemoryType::SYSTEM
                      ? gst_caps_from_string(GVA_TENSORS_CAPS)
                      : gst_caps_from_string(GVA_VAAPI_TENSORS_CAPS);
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

    std::string precision_str;
    if (!precision_to_string(tensor_caps._precision, precision_str))
        return false;
    auto dims_str = dims_to_string(tensor_caps._dims);

    gst_structure_set(structure, "num_tensors", G_TYPE_INT, 1, "types", G_TYPE_STRING, precision_str.c_str(),
                      "dimensions", G_TYPE_STRING, dims_str.c_str(), nullptr);
    return true;
}

TensorCaps::TensorCaps(InferenceBackend::MemoryType memory_type, Precision precision, Layout layout,
                       const std::vector<size_t> &dims)
    : _memory_type(memory_type), _precision(precision), _layout(layout), _dims(dims) {
    if (!precision)
        throw std::invalid_argument("Invalid precision value: " + std::string(precision.name()));

    std::string layout_str;
    if (!layout_to_string(_layout, layout_str))
        throw std::invalid_argument("Failed to convert layout to string: Invalid layout value");

    const auto expected_dims_size = layout_str.size();
    if (_dims.size() != expected_dims_size)
        throw std::runtime_error("Invalid dims size for specified layout '" + layout_str + "'. Expected: " +
                                 std::to_string(expected_dims_size) + " Actual: " + std::to_string(_dims.size()));

    _size = std::accumulate(_dims.begin(), _dims.end(), _precision.size(), safe_mul<size_t>);

    _dims_N_pos = layout_str.find('N');
    _dims_C_pos = layout_str.find('C');
    _dims_H_pos = layout_str.find('H');
    _dims_W_pos = layout_str.find('W');
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
    return _dims_N_pos != std::string::npos;
}

size_t TensorCaps::GetBatchSize() const {
    return HasBatchSize() ? _dims.at(_dims_N_pos) : 0;
}

size_t TensorCaps::GetChannels() const {
    return _dims_C_pos == std::string::npos ? 0 : _dims.at(_dims_C_pos);
}

size_t TensorCaps::GetWidth() const {
    return _dims_W_pos == std::string::npos ? 0 : _dims.at(_dims_W_pos);
}

size_t TensorCaps::GetHeight() const {
    return _dims_H_pos == std::string::npos ? 0 : _dims.at(_dims_H_pos);
}

size_t TensorCaps::GetSize() const {
    return _size;
}

TensorCapsArray TensorCapsArray::FromCaps(const GstCaps *caps) {
    if (!caps)
        throw std::invalid_argument("Invalid capabilities pointer");
    auto memory_type = get_memory_type_from_caps(caps);

    if (gst_caps_get_size(caps) != 1)
        throw std::invalid_argument("Capabilities should have one structure");
    auto structure = gst_caps_get_structure(caps, 0);
    if (!gst_structure_has_name(structure, GVA_TENSOR_MEDIA_NAME))
        throw std::invalid_argument("Expected caps with '" GVA_TENSOR_MEDIA_NAME "' name");

    int num_tensors = 0;
    gst_structure_get_int(structure, "num_tensors", &num_tensors);

    auto precision_str = gst_structure_get_string(structure, "types");
    auto dims_str = gst_structure_get_string(structure, "dimensions");
    if (!precision_str || !dims_str)
        throw std::invalid_argument("Invalid capabilities structure format");
    auto precision_array = Utils::splitString(precision_str, ',');
    auto dims_array = Utils::splitString(dims_str, ',');

    TensorCapsArray result;
    for (int i = 0; i < num_tensors; i++) {
        Precision precision = Precision::UNSPECIFIED;
        if (!string_to_precision(precision_array[i], precision)) {
            throw std::invalid_argument("Invalid precision");
        }
        auto dims = parse_dims_string(dims_array[i]);
        Layout layout = layout_from_dims(dims);
        result._tensor_descs.push_back(TensorCaps(memory_type, precision, layout, dims));
    }

    return result;
}

GstCaps *TensorCapsArray::ToCaps(const TensorCapsArray &tensor_caps_array) {
    auto result = tensor_caps_array.GetMemoryType() == InferenceBackend::MemoryType::SYSTEM
                      ? gst_caps_from_string(GVA_TENSORS_CAPS)
                      : gst_caps_from_string(GVA_VAAPI_TENSORS_CAPS);
    auto structure = gst_caps_get_structure(result, 0);
    if (!structure)
        throw std::invalid_argument("Invalid gst structure pointer");

    int num_tensors = tensor_caps_array.GetTensorNum();
    std::string precision_str;
    std::string dims_str;
    for (int i = 0; i < num_tensors; i++) {
        if (i) {
            precision_str += ",";
            dims_str += ",";
        }
        auto dims = tensor_caps_array.GetTensorDesc(i).GetDims();
        auto precision = tensor_caps_array.GetTensorDesc(i).GetPrecision();
        std::string str;
        if (!precision_to_string(precision, str))
            throw std::runtime_error("Unknown precision");
        precision_str += str;
        dims_str += dims_to_string(dims);
    }

    gst_structure_set(structure, "num_tensors", G_TYPE_INT, num_tensors, "types", G_TYPE_STRING, precision_str.c_str(),
                      "dimensions", G_TYPE_STRING, dims_str.c_str(), nullptr);

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
