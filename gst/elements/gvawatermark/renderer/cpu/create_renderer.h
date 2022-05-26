/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "renderer.h"
#include <dlstreamer/buffer_mapper.h>
#include <dlstreamer/fourcc.h>

std::unique_ptr<Renderer> create_cpu_renderer(dlstreamer::FourCC format, std::shared_ptr<ColorConverter> converter,
                                              dlstreamer::BufferMapperPtr buffer_mapper);
