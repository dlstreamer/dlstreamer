/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "buffer_map/buffer_mapper.h"

#include <level_zero/ze_api.h>

#include <CL/sycl/backend.hpp>
#include <CL/sycl/backend/level_zero.hpp>

#include <memory>

struct ZeDeviceMemContext;

class UsmBufferMapper : public BufferMapper {
    std::shared_ptr<sycl::queue> _queue;

    InferenceBackend::Image _image = {};
    std::unique_ptr<ZeDeviceMemContext> _map_context;

    static void *allocateZeMem(std::shared_ptr<sycl::queue> queue, int dma_fd, size_t dma_size);
    static void mapGstBuffer(GstBuffer *buffer, InferenceBackend::Image &image, ZeDeviceMemContext *map_context);

  public:
    UsmBufferMapper(std::shared_ptr<sycl::queue> queue);
    ~UsmBufferMapper();
    InferenceBackend::Image map(GstBuffer *buffer, GstVideoInfo *info, GstMapFlags) override;
    void unmap() override;
};
