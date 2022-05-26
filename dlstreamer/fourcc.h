/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

namespace dlstreamer {

namespace detail {
template <int a, int b, int c, int d>
struct fourcc {
    enum { code = (a) | (b << 8) | (c << 16) | (d << 24) };
};
} // namespace detail

enum FourCC {
    FOURCC_BGR = detail::fourcc<'B', 'G', 'R', ' '>::code,
    FOURCC_RGB = detail::fourcc<'R', 'G', 'B', ' '>::code,
    FOURCC_BGRX = detail::fourcc<'B', 'G', 'R', 'X'>::code,
    FOURCC_RGBX = detail::fourcc<'R', 'G', 'B', 'X'>::code,
    FOURCC_BGRP = detail::fourcc<'B', 'G', 'R', 'P'>::code,
    FOURCC_RGBP = detail::fourcc<'R', 'G', 'B', 'P'>::code,
    FOURCC_NV12 = detail::fourcc<'N', 'V', '1', '2'>::code,
    FOURCC_I420 = detail::fourcc<'I', '4', '2', '0'>::code,
};

} // namespace dlstreamer
