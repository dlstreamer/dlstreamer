/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include "dlstreamer/fourcc.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace dlstreamer {

enum class MediaType {
    ANY = 0,
    VIDEO = 1,
    // AUDIO = 2,
    TENSORS = 3
};

enum class DataType { U8 = 40, FP32 = 10, I32 = 70 };

class Layout {
  public:
    enum Value {
        ANY = 0,
        CHW = 128, /* for example, single RGBP image */
        HWC = 129, /* for example, single RGB or RGBA image */
        NCHW = 1,  /* for example, batched RGBP images */
        NHWC = 2,  /* for example, batched RGB or RGBA images */
    };

    Layout() {
        set(ANY);
    }
    Layout(Value value) {
        set(value);
    }
    explicit Layout(const std::string &str) {
#define INIT_LAYOUT_VALUE(VALUE)                                                                                       \
    if (str == #VALUE)                                                                                                 \
        set(VALUE);

        set(ANY);
        INIT_LAYOUT_VALUE(CHW);
        INIT_LAYOUT_VALUE(HWC);
        INIT_LAYOUT_VALUE(NCHW);
        INIT_LAYOUT_VALUE(NHWC);
        if (value == ANY)
            throw std::runtime_error("Unknown Layout name " + str);
#undef INIT_LAYOUT_VALUE
    }
    explicit Layout(std::vector<size_t> shape) {
        set(ANY);
        if (shape.size() == 3) {
            if (shape[0] > 4 && shape[1] > 4 && shape[2] <= 4)
                set(HWC);
            if (shape[0] <= 4 && shape[1] > 4 && shape[2] > 4)
                set(CHW);
        } else if (shape.size() == 4) {
            if (shape[1] > 4 && shape[2] > 4 && shape[3] <= 4)
                set(NHWC);
            if (shape[1] <= 4 && shape[2] > 4 && shape[3] > 4)
                set(NCHW);
        }
    }

    inline operator Value() const {
        return value;
    }
    explicit operator bool() {
        return value != ANY;
    }
    std::string to_string() const {
        switch (value) {
        case ANY:
            return "any";
        case NCHW:
            return "NCHW";
        case NHWC:
            return "NHWC";
        case CHW:
            return "CHW";
        case HWC:
            return "HWC";
        }
        throw std::runtime_error("Unknown Layout");
    }
    int w_position() const {
        return w_pos;
    }
    int h_position() const {
        return h_pos;
    }
    int c_position() const {
        return c_pos;
    }
    int n_position() const {
        return n_pos;
    }

  private:
    void set(Value v) {
        value = v;
        std::string str = to_string();
        w_pos = str.find('W');
        h_pos = str.find('H');
        c_pos = str.find('C');
        n_pos = str.find('N');
    }
    Value value = Value::ANY;
    int w_pos = -1;
    int h_pos = -1;
    int c_pos = -1;
    int n_pos = -1;
};

struct PlaneInfo {
    std::vector<size_t> shape;
    std::vector<size_t> stride; // Stride per each shape dimension, ex { 4*width, 4, sizeof(U8) } for HWC/RGBA
    DataType type;
    Layout layout;    // [optional]
    std::string name; // [optional]
    size_t offset;    // [optional]

    PlaneInfo() = default;
    PlaneInfo(std::vector<size_t> shape, DataType type = DataType::U8, std::string name = std::string(),
              std::vector<size_t> stride = {})
        : shape(shape), stride(stride), type(type), name(std::move(name)), offset(0) {
        layout = Layout(shape);
        if (stride.empty())
            set_default_strides();
    }

    size_t width() const {
        return shape.at(layout.w_position());
    }
    size_t height() const {
        return shape.at(layout.h_position());
    }
    size_t channels() const {
        return shape.at(layout.c_position());
    }
    size_t batch() const {
        return shape.at(layout.n_position());
    }

    size_t element_stride() const {
        return stride.at(stride.size() - 1);
    }
    size_t width_stride() const {
        return stride.at(layout.w_position() - 1);
    }
    size_t height_stride() const {
        return stride.at(layout.h_position() - 1);
    }
    size_t channels_stride() const {
        return stride.at(layout.c_position() - 1);
    }

    size_t size() const {
        if (stride.empty() || shape.empty())
            return 0;
        return stride[0] * shape[0];
    }

    inline bool operator<(const PlaneInfo &r) const {
        const PlaneInfo &l = *this;
        return std::tie(l.shape, l.stride, l.type, l.layout, l.name, l.offset) <
               std::tie(r.shape, r.stride, r.type, r.layout, r.name, r.offset);
    }
    inline bool operator==(const PlaneInfo &r) const {
        const PlaneInfo &l = *this;
        return std::tie(l.shape, l.stride, l.type, l.layout, l.name, l.offset) ==
               std::tie(r.shape, r.stride, r.type, r.layout, r.name, r.offset);
    }
    inline bool operator!=(const PlaneInfo &r) const {
        return !operator==(r);
    }

  private:
    void set_default_strides() {
        auto get_type_size = [](DataType p) {
            switch (p) {
            case DataType::U8:
                return 1;
            case DataType::FP32:
                return 4;
            case DataType::I32:
                return 4;
            }
            throw std::runtime_error("Unknown DataType");
        };
        stride.resize(shape.size());
        size_t size = get_type_size(type);
        for (int i = shape.size() - 1; i >= 0; i--) {
            stride[i] = size;
            size *= shape[i];
        }
    }
};

enum class BufferType {
    UNKNOWN = 0,

    // Direct pointers
    CPU = 0x1,
    USM = 0x2,

    // Memory handles
    GST_BUFFER = 0x10,
    VAAPI_SURFACE = 0x20,
    DMA_FD = 0x40,
    OPENCL_BUFFER = 0x80,
    OPENVINO = 0x100,
    OPENCV = 0x200,
};

struct BufferInfo {
    std::vector<PlaneInfo> planes;
    MediaType media_type;
    BufferType buffer_type;
    int format; // Planes format. Media type specific, enum FourCC for 'video' media type

    BufferInfo() : media_type(MediaType::ANY), buffer_type(BufferType(0)), format(0) {
    }
    BufferInfo(MediaType media_type, BufferType buffer_type = BufferType(0), std::vector<PlaneInfo> planes = {})
        : planes(planes), media_type(media_type), buffer_type(buffer_type), format(0) {
    }

    BufferInfo(FourCC fourcc, BufferType buffer_type)
        : media_type(MediaType::VIDEO), buffer_type(buffer_type), format(fourcc){};

    bool operator<(const BufferInfo &r) const {
        const BufferInfo &l = *this;
        return std::tie(l.planes, l.media_type, l.buffer_type, l.format) <
               std::tie(r.planes, r.media_type, r.buffer_type, r.format);
    }

    bool operator==(const BufferInfo &r) const {
        const BufferInfo &l = *this;
        return std::tie(l.planes, l.media_type, l.buffer_type, l.format) ==
               std::tie(r.planes, r.media_type, r.buffer_type, r.format);
    }

    bool operator!=(const BufferInfo &r) const {
        return !operator==(r);
    }
};

using BufferInfoVector = std::vector<BufferInfo>;

// BufferInfoCPtr allows reading BufferInfo but not writing due to 'const'
using BufferInfoCPtr = std::shared_ptr<const BufferInfo>;

} // namespace dlstreamer
