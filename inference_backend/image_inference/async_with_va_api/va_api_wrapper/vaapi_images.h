/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include <algorithm>
#include <future>
#include <memory>
#include <vector>

#include "vaapi_context.h"
#include "vaapi_image_map.h"

namespace InferenceBackend {

struct VaApiImage {
    VaApiContext *context = nullptr;
    Image image = Image();
    std::future<void> sync;
    bool completed = true;
    std::unique_ptr<ImageMap> image_map;

    VaApiImage() = default;
    VaApiImage(const VaApiImage &other) = delete;
    VaApiImage(VaApiContext *context_, int width, int height, int format);
    ~VaApiImage();

    Image Map();
    void Unmap();
};

class VaApiImagePool {
    std::vector<std::unique_ptr<VaApiImage>> _images;
    std::condition_variable _free_image_condition_variable;
    std::mutex _free_images_mutex;
    //    const size_t _pool_size;

  public:
    VaApiImage *AcquireBuffer();
    void ReleaseBuffer(VaApiImage *image);

    void Flush();
    VaApiImagePool(VaApiContext *context_, size_t image_pool_size, int width, int height, int format);
};

} // namespace InferenceBackend
