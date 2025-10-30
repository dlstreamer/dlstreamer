/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "d3d11_context.h"
#include "d3d11_image_map.h"

#include "inference_backend/image.h"
#include "inference_backend/logger.h"

#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

namespace InferenceBackend {

struct D3D11Image {
    D3D11Context *context = nullptr;
    Image image = Image();
    std::future<void> sync;
    bool completed = true;
    std::unique_ptr<ImageMap> image_map;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    Microsoft::WRL::ComPtr<ID3D11Query> gpu_event_query;

    D3D11Image();
    D3D11Image(D3D11Context *context_, uint32_t width, uint32_t height, int format_, MemoryType memory_type);
    ~D3D11Image();

    Image Map();
    void Unmap();
    void WaitForGPU();

    D3D11Image(const D3D11Image &) = delete;
    D3D11Image &operator=(const D3D11Image &) = delete;
};

class D3D11ImagePool {
    std::vector<std::unique_ptr<D3D11Image>> _images;
    std::condition_variable _free_image_condition_variable;
    std::mutex _free_images_mutex;

  public:
    D3D11Image *AcquireBuffer();
    void ReleaseBuffer(D3D11Image *image);
    struct ImageInfo {
        uint32_t width;
        uint32_t height;
        uint32_t batch;
        FourCC format;
        MemoryType memory_type;
    };

    struct SizeParams {
        // Number of items in the pool with default scaling method
        uint32_t num_default = 0;
        // Number of items in the pool with fast scaling method
        uint32_t num_fast = 0;

        SizeParams(uint32_t num_default_scale, uint32_t num_fast_scale) noexcept
            : num_default(num_default_scale), num_fast(num_fast_scale) {
        }
        explicit SizeParams(uint32_t pool_size) noexcept : SizeParams(pool_size, 0) {
        }
        explicit SizeParams() = default;

        // Returns total size (items) of the pool
        size_t size() const {
            return num_default + num_fast;
        }
    };

    D3D11ImagePool(D3D11Context *context, SizeParams size_params, ImageInfo info);

    void Flush();
};

} // namespace InferenceBackend
