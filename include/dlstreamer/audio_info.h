/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <string>

namespace dlstreamer {

struct AudioFormat {
    Format depth : 8; // number valid bits
    Format interleaved : 1;
};

struct AudioInfo {
    AudioInfo(const TensorInfo &info) {
        if (info.shape.size() != 2)
            throw std::runtime_error("Expect Audio tensor with number dimensions equal 2");
        _samples = info.shape[0];
        _channels = info.shape[1];
    }
    size_t samples() {
        return _samples;
    }
    size_t channels() {
        return _channels;
    }

  private:
    size_t _samples;
    size_t _channels;
};

} // namespace dlstreamer
