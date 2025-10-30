/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "d3d11_image_map.h"

#include "inference_backend/logger.h"

using namespace InferenceBackend;

// Initialize static pool
std::shared_ptr<D3D11TexturePool> D3D11ImageMap_SystemMemory::s_texture_pool = nullptr;

// Thread-safe texture pool implementation
D3D11TexturePool::TexturePtr D3D11TexturePool::acquire(ID3D11Device *device, const D3D11_TEXTURE2D_DESC &desc) {
    PoolKey key = std::make_tuple(desc.Width, desc.Height, desc.Format);
    {
        std::lock_guard<std::mutex> lock(pool_mutex);
        auto it = pool.find(key);
        if (it != pool.end()) {
            TexturePtr tex = it->second;
            pool.erase(it);
            GVA_DEBUG("Texture pool HIT: size=%d W=%dx%d Format=%d", pool.size(), desc.Width, desc.Height,
                      (int)desc.Format);
            return tex;
        }
    }

    // Not found in pool, create new
    TexturePtr tex;
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&staging_desc, nullptr, &tex);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create staging texture for CPU readback");
    }
    GVA_DEBUG("Texture pool MISS: created new W=%dx%d Format=%d", desc.Width, desc.Height, (int)desc.Format);
    return tex;
}

void D3D11TexturePool::release(ID3D11Texture2D *tex) {
    if (!tex)
        return;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);
    PoolKey key = std::make_tuple(desc.Width, desc.Height, desc.Format);
    {
        std::lock_guard<std::mutex> lock(pool_mutex);
        if (pool.size() >= MAX_POOL_SIZE) {
            // Remove oldest entry (first in map) to make room
            pool.erase(pool.begin());
        }
        pool[key] = tex;
    }
}

void D3D11TexturePool::clear() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    pool.clear();
}

ImageMap *ImageMap::Create(MemoryType type) {
    ImageMap *map = nullptr;
    switch (type) {
    case MemoryType::SYSTEM:
        map = new D3D11ImageMap_SystemMemory();
        break;
    case MemoryType::D3D11:
        map = new D3D11ImageMap_D3D11Texture();
        break;
    default:
        throw std::invalid_argument("Unsupported format for ImageMap");
    }
    return map;
}

D3D11ImageMap_SystemMemory::D3D11ImageMap_SystemMemory() : num_planes(0) {
}

D3D11ImageMap_SystemMemory::~D3D11ImageMap_SystemMemory() {
    Unmap();
    // Clean up cached resources
    staging_texture.Reset();
    d3d11_device_context.Reset();
}

Image D3D11ImageMap_SystemMemory::Map(const Image &image) {
    Image image_sys = Image();
    image_sys.type = MemoryType::SYSTEM;
    image_sys.width = image.width;
    image_sys.height = image.height;
    image_sys.format = image.format;

    // Get device and context from D3D11Context if available, otherwise fall back to GetImmediateContext
    ID3D11Device *device = static_cast<ID3D11Device *>(image.d3d11_device);
    if (!d3d11_device_context) {
        if (d3d11_context) {
            d3d11_device_context = d3d11_context->DeviceContext();
            d3d11_device_context->AddRef();
        } else {
            device->GetImmediateContext(&d3d11_device_context);
        }
    }

    d3d11_texture = static_cast<ID3D11Texture2D *>(image.d3d11_texture);
    D3D11_TEXTURE2D_DESC desc;
    d3d11_texture->GetDesc(&desc);

    // Use texture pool for staging textures
    bool need_new_staging = !staging_texture;
    if (staging_texture) {
        D3D11_TEXTURE2D_DESC existing_desc;
        staging_texture->GetDesc(&existing_desc);
        if (existing_desc.Width != desc.Width || existing_desc.Height != desc.Height ||
            existing_desc.Format != desc.Format) {
            // Return old texture to pool
            if (s_texture_pool) {
                s_texture_pool->release(staging_texture.Get());
            }
            staging_texture.Reset();
            need_new_staging = true;
        }
    }

    if (need_new_staging) {
        if (s_texture_pool) {
            // Acquire from pool or create new
            staging_texture = s_texture_pool->acquire(device, desc);
        } else {
            // Fallback: create directly if no pool
            D3D11_TEXTURE2D_DESC staging_desc = desc;
            staging_desc.Usage = D3D11_USAGE_STAGING;
            staging_desc.BindFlags = 0;
            staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            staging_desc.MiscFlags = 0;

            HRESULT hr = device->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
            if (FAILED(hr)) {
                throw std::runtime_error("Failed to create staging texture for CPU readback");
            }
        }
    }

    // Lock only for critical D3D11 operations (CopyResource and Map)
    // All DeviceContext calls must be under lock per D3D11/GStreamer requirements
    if (d3d11_context) {
        d3d11_context->Lock();
    }

    // CopyResource queues the GPU work
    d3d11_device_context->CopyResource(staging_texture.Get(), d3d11_texture.Get());

    num_planes = 1;
    if (desc.Format == DXGI_FORMAT_NV12) {
        num_planes = 2;
    }

    // Map the staging texture - this waits for GPU copy to complete
    // We must keep lock held for all DeviceContext operations
    for (int plane = 0; plane < num_planes; ++plane) {
        D3D11_MAPPED_SUBRESOURCE mapped_resource = {};
        HRESULT hr = d3d11_device_context->Map(staging_texture.Get(), plane, D3D11_MAP_READ, 0, &mapped_resource);
        if (FAILED(hr)) {
            if (d3d11_context) {
                d3d11_context->Unlock();
            }
            Unmap();
            throw std::runtime_error("Failed to map staging texture subresource to system memory");
        }
        image_sys.planes[plane] = static_cast<uint8_t *>(mapped_resource.pData);
        image_sys.stride[plane] = mapped_resource.RowPitch;
    }

    if (d3d11_context) {
        d3d11_context->Unlock();
    }
    return image_sys;
}

void D3D11ImageMap_SystemMemory::Unmap() {
    if (staging_texture && d3d11_device_context) {
        if (d3d11_context) {
            d3d11_context->Lock();
        }
        for (int plane = 0; plane < num_planes; ++plane) {
            d3d11_device_context->Unmap(staging_texture.Get(), plane);
        }
        if (d3d11_context) {
            d3d11_context->Unlock();
        }
        num_planes = 0;

        // Return staging texture to pool for reuse
        if (s_texture_pool) {
            s_texture_pool->release(staging_texture.Get());
        }
        staging_texture.Reset();
    }
}

D3D11ImageMap_D3D11Texture::D3D11ImageMap_D3D11Texture() {
}

D3D11ImageMap_D3D11Texture::~D3D11ImageMap_D3D11Texture() {
    Unmap();
}

Image D3D11ImageMap_D3D11Texture::Map(const Image &image) {
    return image;
}

void D3D11ImageMap_D3D11Texture::Unmap() {
}
