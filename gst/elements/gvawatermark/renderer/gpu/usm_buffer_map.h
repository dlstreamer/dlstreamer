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

struct usm_ptr_context_t;
class UsmBufferMapper : public BufferMapper {
    InferenceBackend::Image _image;
    std::shared_ptr<sycl::queue> _queue;
    int _dma_fd;

    void *_ze_context;
    void *_usm_ptr;
    void *get_device_pointer(std::shared_ptr<sycl::queue> queue, GstBuffer *buffer, InferenceBackend::Image *image);

  public:
    UsmBufferMapper(std::shared_ptr<sycl::queue> queue);

    InferenceBackend::Image map(GstBuffer *buffer, GstVideoInfo *info, GstMapFlags) override;
    void unmap() override;
};
