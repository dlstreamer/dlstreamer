/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "d3d11_images.h"
#include "inference_backend/image.h"
#include <d3d11.h>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <wrl/client.h>

namespace InferenceBackend {

// Thread-safe texture pool for staging textures
class D3D11TexturePool {
  public:
    using TexturePtr = Microsoft::WRL::ComPtr<ID3D11Texture2D>;
    static constexpr size_t MAX_POOL_SIZE = 8; // Limit pool size to avoid unbounded memory

    // Key: (width, height, format)
    using PoolKey = std::tuple<UINT, UINT, DXGI_FORMAT>;

    TexturePtr acquire(ID3D11Device *device, const D3D11_TEXTURE2D_DESC &desc);
    void release(ID3D11Texture2D *tex);
    void clear();

  private:
    std::mutex pool_mutex;
    std::map<PoolKey, TexturePtr> pool;
};

class D3D11ImageMap_SystemMemory : public ImageMap {
  public:
    D3D11ImageMap_SystemMemory();
    ~D3D11ImageMap_SystemMemory();

    Image Map(const Image &image) override;
    void Unmap() override;

    void SetContext(D3D11Context *context) {
        d3d11_context = context;
    }

    // Static pool shared across all instances
    static void SetTexturePool(std::shared_ptr<D3D11TexturePool> pool) {
        s_texture_pool = pool;
    }

  protected:
    D3D11Context *d3d11_context = nullptr;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_device_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11_texture;   // Original render target texture
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture; // Staging texture for CPU readback
    int num_planes;

    static std::shared_ptr<D3D11TexturePool> s_texture_pool;
};

class D3D11ImageMap_D3D11Texture : public ImageMap {
  public:
    D3D11ImageMap_D3D11Texture();
    ~D3D11ImageMap_D3D11Texture();

    Image Map(const Image &image) override;
    void Unmap() override;
};

} // namespace InferenceBackend
