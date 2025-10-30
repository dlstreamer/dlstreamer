/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "dlstreamer/d3d11/context.h"
#include "inference_backend/image.h"

#include <d3d11.h>
#include <dxgi.h>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <wrl/client.h>

namespace InferenceBackend {

class D3D11TexturePool; // Forward declaration

class D3D11Context {
  public:
    explicit D3D11Context(ID3D11Device *d3d11_device);
    explicit D3D11Context(dlstreamer::ContextPtr d3d11_device_context);

    ~D3D11Context();

    /* getters */
    ID3D11Device *Device() const {
        return _device.Get();
    }
    ID3D11DeviceContext *DeviceContext() const {
        return _device_context.Get();
    }
    ID3D11VideoDevice *VideoDevice() const {
        return _video_device.Get();
    }
    ID3D11VideoContext *VideoContext() const {
        return _video_context.Get();
    }

    // Static mutex for thread safety
    static std::mutex &GetContextMutex();

    /**
     * Lock the GStreamer D3D11 device for thread-safe access.
     * Must be called before any ID3D11DeviceContext or DXGI operations.
     * Use this instead of GetContextMutex() when working with GStreamer devices.
     */
    void Lock();

    /**
     * Unlock the GStreamer D3D11 device after operations complete.
     */
    void Unlock();

    // Texture pool access
    std::shared_ptr<D3D11TexturePool> GetTexturePool() const {
        return _texture_pool;
    }

    void CreateVideoProcessorAndEnumerator(
        uint32_t input_width, uint32_t input_height, uint32_t output_width, uint32_t output_height,
        Microsoft::WRL::ComPtr<ID3D11VideoProcessor> &video_processor,
        Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> &video_processor_enumerator);

    /**
     * Get or create a cached video processor for common dimensions.
     * Reuses the same processor across multiple frames to avoid creation overhead (5ms per creation).
     * Cache is keyed by (input_w, input_h, output_w, output_h).
     */
    void GetCachedVideoProcessor(uint32_t input_width, uint32_t input_height, uint32_t output_width,
                                 uint32_t output_height, Microsoft::WRL::ComPtr<ID3D11VideoProcessor> &video_processor,
                                 Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> &video_processor_enumerator);

    bool IsPixelFormatSupported(DXGI_FORMAT format) const;

  private:
    dlstreamer::ContextPtr _device_context_storage;
    Microsoft::WRL::ComPtr<ID3D11Device> _device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> _device_context;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> _video_device;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> _video_context;
    std::set<DXGI_FORMAT> _supported_pixel_formats;
    GstD3D11Device *_gst_device = nullptr; // GstD3D11Device* for proper locking

    // Texture pool for staging textures
    std::shared_ptr<D3D11TexturePool> _texture_pool;

    // Video processor cache: key = (input_w, input_h, output_w, output_h)
    struct ProcessorCacheKey {
        uint32_t input_w, input_h, output_w, output_h;
        bool operator<(const ProcessorCacheKey &other) const {
            if (input_w != other.input_w)
                return input_w < other.input_w;
            if (input_h != other.input_h)
                return input_h < other.input_h;
            if (output_w != other.output_w)
                return output_w < other.output_w;
            return output_h < other.output_h;
        }
    };
    struct ProcessorCacheValue {
        Microsoft::WRL::ComPtr<ID3D11VideoProcessor> processor;
        Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> enumerator;
    };
    std::map<ProcessorCacheKey, ProcessorCacheValue> _processor_cache;
    std::mutex _processor_cache_mutex;

    /* private helper methods */
    void create_config_and_contexts();
    void create_supported_pixel_formats();
};

} // namespace InferenceBackend
