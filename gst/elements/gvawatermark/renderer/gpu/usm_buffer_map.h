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

class UsmBufferMapper : public BufferMapper {
    std::shared_ptr<sycl::queue> _queue;
    std::unique_ptr<BufferMapper> _input_mapper;

    static void *getDeviceMemPointer(sycl::queue &queue, int dma_fd, size_t dma_size);
    InferenceBackend::Image convertSurfaceImgToDmaImg(const InferenceBackend::Image &image, size_t *out_dma_size);
    InferenceBackend::Image mapToDmaImg(GstBuffer *buffer, GstMapFlags flags, size_t *out_dma_size);

  public:
    UsmBufferMapper(std::shared_ptr<sycl::queue> queue, std::unique_ptr<BufferMapper> input_buffer_mapper);

    InferenceBackend::MemoryType memoryType() const override;

    InferenceBackend::Image map(GstBuffer *buffer, GstMapFlags flags) override;
    void unmap(InferenceBackend::Image &image) override;
};
