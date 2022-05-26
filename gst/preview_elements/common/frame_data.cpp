/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <config.h>

#include "frame_data.hpp"
#include "vaapi_image_info.hpp"

#include <safe_arithmetic.hpp>

#include <gst/allocators/allocators.h>

#include <numeric>
#include <stdexcept>

namespace {
constexpr static GstMapFlags GST_MAP_VA = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);
}

using namespace InferenceBackend;

FrameData::FrameData() {
    _mapped_frame.buffer = nullptr;
    _tensor_map_info.memory = nullptr;
}

FrameData::~FrameData() {
    Unmap();
}

void FrameData::Map(GstBuffer *buffer, GstVideoInfo *video_info, MemoryType memory_type, GstMapFlags map_flags) {
    Unmap();

    if (!buffer)
        throw std::invalid_argument("GstBuffer is null");
    if (!video_info)
        throw std::invalid_argument("GstVideoInfo is absent during GstBuffer mapping");

    guint n_planes = GST_VIDEO_INFO_N_PLANES(video_info);
    if (n_planes == 0 || n_planes > MAX_PLANES_NUM)
        throw std::logic_error("Image planes number " + std::to_string(n_planes) + " isn't supported");

    switch (memory_type) {
    case MemoryType::SYSTEM: {
        if (!gst_video_frame_map(&_mapped_frame, video_info, buffer, map_flags))
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        break;
    }
    case MemoryType::VAAPI: {
        GstMapFlags flags = GST_MAP_VA;
        GstMapInfo map_info;
        if (!gst_buffer_map(buffer, &map_info, flags)) {
            flags = static_cast<GstMapFlags>(flags | GST_MAP_READ);
            if (!gst_buffer_map(buffer, &map_info, flags)) {
                throw std::runtime_error("Couldn't map buffer (VAAPI memory)");
            }
        }
        auto surface = *reinterpret_cast<const uint32_t *>(map_info.data);
        gst_buffer_unmap(buffer, &map_info);
        if (surface == VASURFACE_INVALID_ID)
            throw std::runtime_error("Got invalid surface after map (VAAPI memory)");
        _va_surface_id = surface;
        break;
    }
    case MemoryType::DMA_BUFFER: {
        auto mem = gst_buffer_peek_memory(buffer, 0);
        if (mem == nullptr)
            throw std::runtime_error("Failed to map GstBuffer: memory is invalid");
        _dma_fd = gst_dmabuf_memory_get_fd(mem);
        if (_dma_fd < 0)
            throw std::runtime_error("Failed to map GstBuffer: DMA buffer FD is invalid");
        break;
    }
    default:
        throw std::runtime_error("Failed to map GstBuffer: unknown memory type requested");
    }

    _width = safe_convert<uint32_t>(GST_VIDEO_INFO_WIDTH(video_info));
    _height = safe_convert<uint32_t>(GST_VIDEO_INFO_HEIGHT(video_info));
    _format = gst_format_to_four_CC(GST_VIDEO_INFO_FORMAT(video_info));

    _mem_type = memory_type;
    _mapped_buffer = buffer;

    if (IsVideoFrameMapped()) {
        for (auto i = 0u; i < GST_VIDEO_FRAME_N_PLANES(&_mapped_frame); i++) {
            _planes.push_back(reinterpret_cast<uint8_t *>(GST_VIDEO_FRAME_PLANE_DATA(&_mapped_frame, i)));
            _strides.push_back(safe_convert<uint32_t>(GST_VIDEO_FRAME_PLANE_STRIDE(&_mapped_frame, i)));
            _offsets.push_back(safe_convert<uint32_t>(GST_VIDEO_FRAME_PLANE_OFFSET(&_mapped_frame, i)));
        }
        _size = safe_convert<uint32_t>(GST_VIDEO_FRAME_SIZE(&_mapped_frame));
    }
}

// TODO: split video and tensor mapping (two clases, maybe same interface)
void FrameData::Map(GstBuffer *buffer, const TensorCaps &tensor_caps, GstMapFlags map_flags, MemoryType memory_type,
                    uint32_t planes_num, std::vector<size_t> planes_sizes) {
    Unmap();

    if (!buffer)
        throw std::invalid_argument("GstBuffer is null");
    if (!planes_sizes.empty() && planes_num != planes_sizes.size())
        throw std::invalid_argument("Number of planes and planes sizes is different");

    switch (memory_type) {
    case MemoryType::SYSTEM: {
        if (!gst_buffer_map(buffer, &_tensor_map_info, map_flags))
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        break;
    }
#ifdef ENABLE_VAAPI
    case MemoryType::VAAPI: {
        auto vaapi_image_info = reinterpret_cast<VaapiImageInfo *>(
            gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VaApiImage")));
        if (!vaapi_image_info)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaapiImageInfo");
        // Unmap called on quark destroy
        _va_surface_id = vaapi_image_info->image->Map().va_surface_id;
        if (_va_surface_id == VASURFACE_INVALID_ID)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaSurfaceId");
        break;
    }
#endif
    default:
        throw std::runtime_error("Unsupported memory type to map tensor data");
    }

    _width = safe_convert<uint32_t>(tensor_caps.GetWidth());
    _height = safe_convert<uint32_t>(tensor_caps.GetHeight());
    // TODO: Currently used in opencv preproc only. Actually tells that format is planar, and not always RGBP
    _format = FOURCC_RGBP;

    _mem_type = memory_type;
    _tensor_caps = tensor_caps;
    _mapped_buffer = buffer;

    if (IsTensorMapped()) {
        if (planes_sizes.empty()) {
            // Assume default plane size as W * H
            planes_sizes.reserve(planes_num);
            auto default_plane_size = safe_mul(_height, _width);
            for (auto i = 0u; i < planes_num; i++)
                planes_sizes.push_back(default_plane_size);
        }

        auto offset = 0u;
        for (auto plane_size : planes_sizes) {
            _planes.push_back(_tensor_map_info.data + offset);
            _strides.push_back(0);
            _offsets.push_back(0);
            offset += plane_size;
        }
        _size = safe_convert<uint32_t>(_tensor_map_info.size);
    }
}

bool FrameData::IsMapped() const {
    return _mapped_buffer != nullptr;
}

void FrameData::Unmap() {
    if (_mapped_frame.buffer) {
        gst_video_frame_unmap(&_mapped_frame);
        _mapped_frame.buffer = nullptr;
    }
    if (_tensor_map_info.memory) {
        gst_buffer_unmap(_mapped_buffer, &_tensor_map_info);
        _tensor_map_info.memory = nullptr;
    }
    _mapped_buffer = nullptr;
    _tensor_caps = {};
}

MemoryType FrameData::GetMemoryType() const {
    return _mem_type;
}

int FrameData::GetDMABufDescriptor() const {
    return _dma_fd;
}

uint32_t FrameData::GetVaSurfaceID() const {
    return _va_surface_id;
}

uint32_t FrameData::GetPlanesNum() const {
    return _planes.size();
}

uint8_t *FrameData::GetPlane(uint32_t index) const {
    if (index >= GetPlanesNum())
        return nullptr;
    return _planes.at(index);
}

uint32_t FrameData::GetOffset(uint32_t index) const {
    if (index >= GetPlanesNum())
        return 0;

    return _offsets.at(index);
}

uint32_t FrameData::GetStride(uint32_t index) const {
    if (index >= GetPlanesNum())
        return 0;

    return _strides.at(index);
}

const std::vector<uint8_t *> &FrameData::GetPlanes() const {
    return _planes;
}

const std::vector<uint32_t> &FrameData::GetStrides() const {
    return _strides;
}

const std::vector<uint32_t> &FrameData::GetOffsets() const {
    return _offsets;
}

uint32_t FrameData::GetSize() const {
    return _size;
}

uint32_t FrameData::GetWidth() const {
    return _width;
}

uint32_t FrameData::GetHeight() const {
    return _height;
}

FourCC FrameData::GetFormat() const {
    return _format;
}

Precision FrameData::GetPrecision() const {
    if (IsTensorMapped())
        return _tensor_caps.GetPrecision();

    return Precision::UNSPECIFIED;
}

Layout FrameData::GetLayout() const {
    if (IsTensorMapped())
        return _tensor_caps.GetLayout();

    return Layout::ANY;
}

bool FrameData::IsVideoFrameMapped() const {
    return _mapped_frame.buffer != nullptr;
}

bool FrameData::IsTensorMapped() const {
    return _tensor_map_info.memory != nullptr;
}
