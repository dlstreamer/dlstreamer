/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <memory_type.hpp>

#include <inference_backend/image.h>

#include <ie_core.hpp>

#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstdint>
#include <string>
#include <vector>

using Layout = InferenceEngine::Layout;
using Precision = InferenceEngine::Precision;

bool layout_to_string(Layout layout, std::string &res);
bool string_to_layout(const std::string &str, Layout &res);

bool precision_to_string(Precision precision, std::string &res);
bool string_to_precision(const std::string &str, Precision &res);

bool string_to_format(const std::string &str, int &res);

InferenceBackend::FourCC gst_format_to_four_CC(GstVideoFormat format);
uint16_t get_channels_count(int video_format);

class TensorCaps {
  public:
    static TensorCaps FromCaps(const GstCaps *caps);
    static TensorCaps FromStructure(const GstStructure *structure, InferenceBackend::MemoryType memory_type);
    static GstCaps *ToCaps(const TensorCaps &tensor_caps);
    static bool ToStructure(const TensorCaps &tensor_caps, GstStructure *structure);

    TensorCaps() = default;
    TensorCaps(InferenceBackend::MemoryType memory_type, Precision precision, Layout layout,
               const std::vector<size_t> &dims);

    InferenceBackend::MemoryType GetMemoryType() const;
    Precision GetPrecision() const;
    Layout GetLayout() const;

    // TODO: should not be here, need to be more abstract regarding dims
    bool HasBatchSize() const;
    size_t GetBatchSize() const;
    size_t GetChannels() const;
    size_t GetWidth() const;
    size_t GetHeight() const;

    std::vector<size_t> GetDims() const;
    size_t GetSize() const;

  private:
    InferenceBackend::MemoryType _memory_type = InferenceBackend::MemoryType::SYSTEM;
    Precision _precision = Precision::UNSPECIFIED;
    Layout _layout = Layout::ANY;
    std::vector<size_t> _dims;

    size_t _size = 0;
    size_t _dims_N_pos = std::string::npos;
    size_t _dims_C_pos = std::string::npos;
    size_t _dims_H_pos = std::string::npos;
    size_t _dims_W_pos = std::string::npos;
};

class TensorCapsArray {
  public:
    static TensorCapsArray FromCaps(const GstCaps *caps);
    static GstCaps *ToCaps(const TensorCapsArray &tensor_caps_array);

    TensorCapsArray() = default;
    TensorCapsArray(const std::vector<TensorCaps> &tensor_descs);

    size_t GetTensorNum() const;
    const TensorCaps &GetTensorDesc(size_t index) const;
    InferenceBackend::MemoryType GetMemoryType() const;

  private:
    std::vector<TensorCaps> _tensor_descs;
};
