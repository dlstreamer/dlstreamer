/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <memory_type.hpp>

#include <inference_backend/image.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstdint>
#include <string>

// TODO: maybe move out to common?
enum class Layout { ANY = 0, NCHW = 1, NHWC = 2, NC = 193, CHW = 128 };
enum class Precision { UNSPECIFIED = 255, FP32 = 10, U8 = 40 };

bool layout_to_string(Layout layout, std::string &res);
bool string_to_layout(const std::string &str, Layout &res);

bool precision_to_string(Precision precision, std::string &res);
bool string_to_precision(const std::string &str, Precision &res);

bool string_to_format(const std::string &str, int &res);

InferenceBackend::FourCC gst_format_to_four_CC(GstVideoFormat format);

class TensorCaps {
  public:
    TensorCaps() = default;
    TensorCaps(const GstCaps *caps);

    InferenceBackend::MemoryType GetMemoryType() const;
    Precision GetPrecision() const;
    Layout GetLayout() const;

    bool HasBatchSize() const;
    int GetBatchSize() const;

    bool HasChannels() const;
    int GetChannels() const;

    int GetDimension(size_t index) const;

  private:
    InferenceBackend::MemoryType _memory_type = InferenceBackend::MemoryType::SYSTEM;
    Precision _precision = Precision::UNSPECIFIED;
    Layout _layout = Layout::ANY;
    int _batch_size = -1;
    int _channels = -1;
    int _dimension1 = -1;
    int _dimension2 = -1;
};
