/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "renderer.h"
#include <dlstreamer/base/memory_mapper.h>
#include <dlstreamer/image_info.h>

std::unique_ptr<Renderer> create_cpu_renderer(dlstreamer::ImageFormat format, std::shared_ptr<ColorConverter> converter,
                                              dlstreamer::MemoryMapperPtr buffer_mapper);
