/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"

#include "dlstreamer/audio_info.h"
#include "dlstreamer/image_info.h"

namespace dlstreamer {

struct FrameInfo {
    TensorInfoVector tensors;
    MediaType media_type;
    MemoryType memory_type;
    Format format;

    FrameInfo() : media_type(MediaType::Any), memory_type(MemoryType::Any), format(0) {
    }

    FrameInfo(MediaType media_type, MemoryType memory_type = MemoryType::Any, TensorInfoVector tensors = {})
        : tensors(tensors), media_type(media_type), memory_type(memory_type), format(0) {
    }

    FrameInfo(ImageFormat image_format, MemoryType memory_type = MemoryType::CPU, TensorInfoVector tensors = {})
        : tensors(tensors), media_type(MediaType::Image), memory_type(memory_type),
          format(static_cast<Format>(image_format)){};

    bool operator<(const FrameInfo &r) const {
        const FrameInfo &l = *this;
        return std::tie(l.tensors, l.media_type, l.memory_type, l.format) <
               std::tie(r.tensors, r.media_type, r.memory_type, r.format);
    }

    bool operator==(const FrameInfo &r) const {
        const FrameInfo &l = *this;
        return std::tie(l.tensors, l.media_type, l.memory_type, l.format) ==
               std::tie(r.tensors, r.media_type, r.memory_type, r.format);
    }

    bool operator!=(const FrameInfo &r) const {
        return !operator==(r);
    }
};

using FrameInfoVector = std::vector<FrameInfo>;

} // namespace dlstreamer
