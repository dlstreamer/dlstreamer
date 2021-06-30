/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "types.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace {
const std::map<std::string, Layout> string_to_layout_map = {
    {"ANY", Layout::ANY}, {"NCHW", Layout::NCHW}, {"NHWC", Layout::NHWC}, {"CHW", Layout::CHW}, {"NC", Layout::NC}};

const std::map<std::string, Precision> string_to_precision_map = {
    {"UNSPECIFIED", Precision::UNSPECIFIED}, {"U8", Precision::U8}, {"FP32", Precision::FP32}};
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

TensorCaps::TensorCaps(const GstCaps *caps) {
    if (gst_caps_get_size(caps) != 1)
        throw std::invalid_argument("Capabilities should have one structure");

    _memory_type = get_memory_type_from_caps(caps);

    auto structure = gst_caps_get_structure(caps, 0);
    if (!gst_structure_has_name(structure, "application/tensor"))
        throw std::invalid_argument("Capabilities are not 'application/tensor' type");

    auto precision_str = gst_structure_get_string(structure, "precision");
    auto layout_str = gst_structure_get_string(structure, "layout");

    if (!string_to_precision(precision_str, _precision) || !string_to_layout(layout_str, _layout)) {
        throw std::invalid_argument("Invalid capabilities structure format: failed to get precision and layout");
    }

    // Get batch-size and channels
    switch (_layout) {
    case Layout::NC:
    case Layout::NCHW:
    case Layout::NHWC:
        if (!gst_structure_get_int(structure, "batch-size", &_batch_size))
            throw std::invalid_argument("Invalid capabilities structure format: failed to get batch-size");
        /* fall through */
    case Layout::CHW:
        if (!gst_structure_get_int(structure, "channels", &_channels))
            throw std::invalid_argument("Invalid capabilities structure format: failed to get channels");
        break;
    default:
        break;
    }

    // Get dimensions
    switch (_layout) {
    case Layout::NCHW:
    case Layout::NHWC:
    case Layout::CHW:
        if (!gst_structure_get_int(structure, "dimension1", &_dimension1) ||
            !gst_structure_get_int(structure, "dimension2", &_dimension2)) {
            throw std::invalid_argument("Invalid capabilities structure format: failed to get dimensions");
        }
        break;
    case Layout::NC:
    default:
        break;
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

bool TensorCaps::HasBatchSize() const {
    return _batch_size >= 0;
}

int TensorCaps::GetBatchSize() const {
    return _batch_size;
}

bool TensorCaps::HasChannels() const {
    return _channels >= 0;
}

int TensorCaps::GetChannels() const {
    return _channels;
}

int TensorCaps::GetDimension(size_t index) const {
    switch (index) {
    case 1:
        return _dimension1;
    case 2:
        return _dimension2;
    default:
        throw std::invalid_argument("No specified dimension is found in capabilities.");
    }
}
