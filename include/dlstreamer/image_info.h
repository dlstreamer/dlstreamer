/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <string>

namespace dlstreamer {

namespace detail {
template <int a, int b, int c, int d>
struct fourcc {
    enum { code = (a) | (b << 8) | (c << 16) | (d << 24) };
};
} // namespace detail

enum class ImageFormat : Format {
    BGR = detail::fourcc<'B', 'G', 'R', ' '>::code,
    RGB = detail::fourcc<'R', 'G', 'B', ' '>::code,
    BGRX = detail::fourcc<'B', 'G', 'R', 'X'>::code,
    RGBX = detail::fourcc<'R', 'G', 'B', 'X'>::code,
    BGRP = detail::fourcc<'B', 'G', 'R', 'P'>::code,
    RGBP = detail::fourcc<'R', 'G', 'B', 'P'>::code,
    NV12 = detail::fourcc<'N', 'V', '1', '2'>::code,
    I420 = detail::fourcc<'I', '4', '2', '0'>::code,
};

static inline std::string image_format_to_string(ImageFormat format) {
    switch (format) {
    case ImageFormat::BGR:
        return "BGR";
    case ImageFormat::RGB:
        return "RGB";
    case ImageFormat::BGRX:
        return "BGRX";
    case ImageFormat::RGBX:
        return "RGBX";
    case ImageFormat::BGRP:
        return "BGRP";
    case ImageFormat::RGBP:
        return "RGBP";
    case ImageFormat::NV12:
        return "NV12";
    case ImageFormat::I420:
        return "I420";
    };
    return "Unknown format: " + std::to_string(int(format));
}

class ImageLayout {
  public:
    enum Value {
        Any = 0,
        HW = 192,
        CHW = 128, /* for example, single RGBP image */
        HWC = 129, /* for example, single RGB or RGBA image */
        NCHW = 1,  /* for example, batched RGBP images */
        NHWC = 2,  /* for example, batched RGB or RGBA images */
    };

    ImageLayout() {
        set(Any);
    }
    ImageLayout(Value value) {
        set(value);
    }
    explicit ImageLayout(const std::string &str) {
        if (str == "HW")
            set(HW);
        if (str == "CHW")
            set(CHW);
        else if (str == "HWC")
            set(HWC);
        else if (str == "NCHW")
            set(NCHW);
        else if (str == "NHWC")
            set(NHWC);
        else
            throw std::runtime_error("Unknown ImageLayout name " + str);
    }
    explicit ImageLayout(std::vector<size_t> shape) {
        set(Any);
        if (shape.size() == 2) {
            set(HW);
        } else if (shape.size() == 3) {
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
        return value != Any;
    }
    std::string to_string() const {
        switch (value) {
        case Any:
            return "Any";
        case HW:
            return "HW";
        case NCHW:
            return "NCHW";
        case NHWC:
            return "NHWC";
        case CHW:
            return "CHW";
        case HWC:
            return "HWC";
        }
        throw std::runtime_error("Unknown ImageLayout");
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
    Value value = Value::Any;
    int w_pos = -1;
    int h_pos = -1;
    int c_pos = -1;
    int n_pos = -1;
};

struct ImageInfo {
    ImageInfo(const TensorInfo &info) : _info(info), _layout(ImageLayout(info.shape)) {
    }

    ImageLayout layout() const {
        return _layout;
    }
    size_t width() const {
        return _info.shape.at(_layout.w_position());
    }
    size_t height() const {
        return _info.shape.at(_layout.h_position());
    }
    size_t channels() const {
        auto pos = _layout.c_position();
        return (pos >= 0) ? _info.shape.at(pos) : 1;
    }
    size_t batch() const {
        auto pos = _layout.n_position();
        return (pos >= 0) ? _info.shape.at(pos) : 1;
    }

    size_t width_stride() const {
        return _info.stride.at(_layout.w_position() - 1);
    }
    size_t height_stride() const {
        return _info.stride.at(_layout.h_position() - 1);
    }
    size_t channels_stride() const {
        return _info.stride.at(_layout.c_position() - 1);
    }

    const TensorInfo info() const {
        return _info;
    };

  private:
    const TensorInfo &_info;
    const ImageLayout _layout;
};

} // namespace dlstreamer
