/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "renderer.h"

std::unique_ptr<Renderer> create_cpu_renderer(const GstVideoInfo *video_info, std::shared_ptr<ColorConverter> converter,
                                              InferenceBackend::MemoryType memory_type);
