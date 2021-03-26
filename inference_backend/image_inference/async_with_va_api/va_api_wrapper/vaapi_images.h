/*******************************************************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_context.h"
#include "vaapi_image_map.h"
#include "vaapi_utils.h"

#include "inference_backend/image.h"

#include <algorithm>
#include <future>
#include <memory>
#include <vector>

namespace InferenceBackend {

struct VaApiImage {
    VaApiContext *context = nullptr;
    Image image = Image();
    std::future<void> sync;
    bool completed = true;
    std::unique_ptr<ImageMap> image_map;

    VaApiImage();
    VaApiImage(const VaApiImage &other) = delete;
    VaApiImage(VaApiContext *context_, uint32_t width, uint32_t height, int format, MemoryType memory_type);
    ~VaApiImage();

    Image Map();
    void Unmap();
};

class VaApiImagePool {
    std::vector<std::unique_ptr<VaApiImage>> _images;
    std::condition_variable _free_image_condition_variable;
    std::mutex _free_images_mutex;

  public:
    VaApiImage *AcquireBuffer();
    void ReleaseBuffer(VaApiImage *image);
    struct ImageInfo {
        uint32_t width;
        uint32_t height;
        FourCC format;
        MemoryType memory_type;
    };
    void Flush();
    VaApiImagePool(VaApiContext *context_, size_t image_pool_size, ImageInfo info);
};

} // namespace InferenceBackend
