/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_info.h"
#include "dlstreamer/context.h"
#include "dlstreamer/dictionary.h"

#include <assert.h>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace dlstreamer {

class Buffer {
  public:
    using handle_t = intptr_t;
    static constexpr auto pts_id = "pts"; // handle(pts_id) returns PTS in nano-seconds (if available)

    Buffer() = default;
    using SliceInfo = std::vector<std::pair<size_t, size_t>>; // ex, {{y_min, y_max}, {x_min, x_max}, {c_min, c_max}}
    Buffer(const Buffer &buffer, const SliceInfo &slice);     // slice (crop) of another buffer
    virtual ~Buffer() {
    }

    virtual BufferType type() const = 0;

    virtual void *data(size_t plane_index = 0) const = 0;

    virtual std::vector<std::string> keys() const = 0;

    virtual handle_t handle(std::string const &handle_id, size_t plane_index = 0) const = 0;

    virtual handle_t handle(std::string const &handle_id, size_t plane_index, size_t default_value) const noexcept = 0;

    virtual BufferInfoCPtr info() const = 0;

    virtual ContextPtr context() const = 0;

    virtual DictionaryVector metadata() const = 0;

    virtual DictionaryPtr add_metadata(const std::string &name) = 0;

    virtual void remove_metadata(DictionaryPtr meta) = 0;

    virtual void add_handle(std::string handle_id, size_t plane_index, handle_t handle) = 0;
};

using BufferPtr = std::shared_ptr<Buffer>;

template <typename T>
std::shared_ptr<T> BufferSlice(std::shared_ptr<T> buffer_ptr, const Buffer::SliceInfo &slice) {
    return std::make_shared<T>(new T(*buffer_ptr, slice), [buffer_ptr](T *b) { delete b; });
}

inline std::string_view buffer_type_to_string(BufferType type) {
    switch (type) {
    case BufferType::CPU:
        return "System";
    case BufferType::GST_BUFFER:
        return "GStreamer";
    case BufferType::VAAPI_SURFACE:
        return "VASurface";
    case BufferType::DMA_FD:
        return "DMABuf";
    case BufferType::USM:
        return "USM";
    case BufferType::OPENCL_BUFFER:
        return "OpenCL";
    case BufferType::OPENVINO:
        return "OpenVINO";
    case BufferType::OPENCV:
        return "OpenCV";
    case BufferType::UNKNOWN:
        return "UNKNOWN";
    }
    throw std::runtime_error("Unknown BufferType");
}

static inline BufferType buffer_type_from_string(std::string str) {
    if (str == "System" || str == "SystemMemory")
        return BufferType::CPU;
    if (str == "GStreamer")
        return BufferType::GST_BUFFER;
    if (str == "VASurface")
        return BufferType::VAAPI_SURFACE;
    if (str == "DMABuf")
        return BufferType::DMA_FD;
    if (str == "USM")
        return BufferType::USM;
    if (str == "OpenCL")
        return BufferType::OPENCL_BUFFER;
    if (str == "OpenVINO")
        return BufferType::OPENVINO;
    if (str == "UNKNOWN")
        return BufferType::UNKNOWN;
    throw std::runtime_error("Unknown BufferType string");
}

} // namespace dlstreamer
