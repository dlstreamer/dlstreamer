/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "d3d11_context.h"
#include "d3d11_image_map.h"

#include "inference_backend/logger.h"

#include <cassert>
#include <map>
#include <mutex>
#include <vector>

#include <fcntl.h>
#include <gst/d3d11/gstd3d11device.h>

using namespace InferenceBackend;

// Static mutex for thread-safe D3D11 context operations
static std::mutex g_d3d11_context_mutex;

std::mutex &D3D11Context::GetContextMutex() {
    return g_d3d11_context_mutex;
}

D3D11Context::D3D11Context(ID3D11Device *d3d11_device) : _device(d3d11_device) {
    create_config_and_contexts();
    create_supported_pixel_formats();
    // Initialize texture pool
    _texture_pool = std::make_shared<D3D11TexturePool>();
    D3D11ImageMap_SystemMemory::SetTexturePool(_texture_pool);
}

D3D11Context::D3D11Context(dlstreamer::ContextPtr display_context) : _device_context_storage(display_context) {

    auto gst_device = static_cast<GstD3D11Device *>(display_context->handle(dlstreamer::D3D11Context::key::d3d_device));
    _gst_device = gst_device; // Store for Lock/Unlock
    _device = gst_d3d11_device_get_device_handle(gst_device);

    create_config_and_contexts();
    create_supported_pixel_formats();
    // Initialize texture pool
    _texture_pool = std::make_shared<D3D11TexturePool>();
    D3D11ImageMap_SystemMemory::SetTexturePool(_texture_pool);
}

D3D11Context::~D3D11Context() {
}

bool D3D11Context::IsPixelFormatSupported(DXGI_FORMAT format) const {
    return _supported_pixel_formats.count(format);
}

void D3D11Context::Lock() {
    if (_gst_device) {
        gst_d3d11_device_lock(_gst_device);
    } else {
        g_d3d11_context_mutex.lock();
    }
}

void D3D11Context::Unlock() {
    if (_gst_device) {
        gst_d3d11_device_unlock(_gst_device);
    } else {
        g_d3d11_context_mutex.unlock();
    }
}

/**
 * Creates D3D11 video processing interfaces and context.
 * Initializes immediate context, video device, video context.
 * Video processors are created on-demand with specific dimensions.
 *
 * @pre _device must be set and initialized.
 * @post _device_context, _video_device, _video_context are set.
 *
 * @throw std::invalid_argument if D3D11 device is invalid or video interfaces cannot be created.
 */
void D3D11Context::create_config_and_contexts() {
    assert(_device);

    // Get immediate context
    _device->GetImmediateContext(_device_context.GetAddressOf());
    if (!_device_context)
        throw std::invalid_argument("Could not get D3D11 immediate context");

    // Get video device interface (for video processing)
    HRESULT hr = _device.As(&_video_device);
    if (FAILED(hr) || !_video_device)
        throw std::invalid_argument("Could not get D3D11 video device interface.");

    // Get video context interface
    Lock();
    hr = _device_context.As(&_video_context);
    if (FAILED(hr) || !_video_context)
        throw std::invalid_argument("Could not get D3D11 video context interface");
    Unlock();
}

void D3D11Context::CreateVideoProcessorAndEnumerator(
    uint32_t input_width, uint32_t input_height, uint32_t output_width, uint32_t output_height,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> &video_processor,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> &video_processor_enumerator) {

    if (!_video_device) {
        throw std::runtime_error("Video device not initialized");
    }

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc = {};
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputWidth = input_width;
    content_desc.InputHeight = input_height;
    content_desc.OutputWidth = output_width;
    content_desc.OutputHeight = output_height;
    content_desc.Usage = D3D11_VIDEO_USAGE_OPTIMAL_SPEED;

    // Set frame rates (200 fps as default)
    content_desc.InputFrameRate.Numerator = 0;
    content_desc.InputFrameRate.Denominator = 0;
    content_desc.OutputFrameRate.Numerator = 0;
    content_desc.OutputFrameRate.Denominator = 0;

    // Create enumerator
    HRESULT hr =
        _video_device->CreateVideoProcessorEnumerator(&content_desc, video_processor_enumerator.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create video processor enumerator");
    }

    // Create processor using the same enumerator
    hr = _video_device->CreateVideoProcessor(video_processor_enumerator.Get(), 0, video_processor.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create video processor");
    }
}

void D3D11Context::GetCachedVideoProcessor(
    uint32_t input_width, uint32_t input_height, uint32_t output_width, uint32_t output_height,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> &video_processor,
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> &video_processor_enumerator) {

    ProcessorCacheKey key = {input_width, input_height, output_width, output_height};

    {
        std::lock_guard<std::mutex> lock(_processor_cache_mutex);
        auto it = _processor_cache.find(key);
        if (it != _processor_cache.end()) {
            video_processor = it->second.processor;
            video_processor_enumerator = it->second.enumerator;
            GVA_DEBUG("D3D11 VideoProcessor cache HIT: %ux%u -> %ux%u", input_width, input_height, output_width,
                      output_height);
            return;
        }
    }

    // Cache miss - create new processor
    GVA_DEBUG("D3D11 VideoProcessor cache MISS: %ux%u -> %ux%u, creating new", input_width, input_height, output_width,
              output_height);
    CreateVideoProcessorAndEnumerator(input_width, input_height, output_width, output_height, video_processor,
                                      video_processor_enumerator);

    // Store in cache
    {
        std::lock_guard<std::mutex> lock(_processor_cache_mutex);
        ProcessorCacheValue val;
        val.processor = video_processor;
        val.enumerator = video_processor_enumerator;
        _processor_cache[key] = val;
    }
}

/**
 * Creates a set of formats supported by D3D11 video processor.
 * Uses a temporary enumerator to check format support.
 *
 * @pre _video_device must be set and initialized.
 * @post _supported_pixel_formats is set.
 *
 * @throw std::runtime_error if video_device is not initialized.
 */
void D3D11Context::create_supported_pixel_formats() {
    if (!_video_device) {
        throw std::runtime_error("Video device not initialized");
    }

    // Create a temporary enumerator to check format support
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> temp_processor;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> temp_enumerator;
    CreateVideoProcessorAndEnumerator(1920, 1080, 1920, 1080, temp_processor, temp_enumerator);

    // List of common DXGI formats to test for video processing support
    std::vector<DXGI_FORMAT> formats_to_test = {DXGI_FORMAT_NV12,
                                                DXGI_FORMAT_YUY2,
                                                DXGI_FORMAT_AYUV,
                                                DXGI_FORMAT_Y410,
                                                DXGI_FORMAT_Y416,
                                                DXGI_FORMAT_P010,
                                                DXGI_FORMAT_P016,
                                                DXGI_FORMAT_420_OPAQUE,
                                                DXGI_FORMAT_B8G8R8A8_UNORM,
                                                DXGI_FORMAT_R8G8B8A8_UNORM,
                                                DXGI_FORMAT_B8G8R8X8_UNORM,
                                                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                                                DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
                                                DXGI_FORMAT_R10G10B10A2_UNORM,
                                                DXGI_FORMAT_R16G16B16A16_FLOAT};

    // Check each format for video processor support
    for (DXGI_FORMAT format : formats_to_test) {
        UINT flags = 0;
        HRESULT hr = temp_enumerator->CheckVideoProcessorFormat(format, &flags);

        if (SUCCEEDED(hr) && (flags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) {
            _supported_pixel_formats.insert(format);
        }
    }
}
