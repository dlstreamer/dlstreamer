/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "frame_data.hpp"

#include <gst/allocators/allocators.h>

#include <stdexcept>

using namespace InferenceBackend;

VaMemInfo get_va_info_from_buffer(GstBuffer *buffer) {
    VaMemInfo result;
    result.va_display = gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VADisplay"));
    result.va_surface_id = reinterpret_cast<uintptr_t>(
        gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VASurfaceID")));
    return result;
}

FrameData::FrameData() {
    _mapped_frame.buffer = nullptr;
    _tensor_map_info.memory = nullptr;
}

FrameData::~FrameData() {
    Unmap();
}

void FrameData::Map(GstBuffer *buffer, GstVideoInfo *video_info, MemoryType memory_type, GstMapFlags map_flags) {
    Unmap();

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
        _va_mem_info = get_va_info_from_buffer(buffer);
        if (_va_mem_info.va_display == nullptr)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaDisplay");
        if ((int)_va_mem_info.va_surface_id < 0)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaSurfaceId");
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

    _width = GST_VIDEO_INFO_WIDTH(video_info);
    _height = GST_VIDEO_INFO_HEIGHT(video_info);
    _format = gst_format_to_four_CC(GST_VIDEO_INFO_FORMAT(video_info));

    _mem_type = memory_type;
    _mapped_buffer = buffer;
}

// TODO: split video and tensor mapping (two clases, maybe same interface)
void FrameData::Map(GstBuffer *buffer, const TensorCaps &tensor_caps, GstMapFlags map_flags, int planes_num,
                    MemoryType memory_type) {
    Unmap();

    if (planes_num < 0)
        throw std::invalid_argument("Number of planes is invalid");

    switch (memory_type) {
    case MemoryType::SYSTEM: {
        if (!gst_buffer_map(buffer, &_tensor_map_info, map_flags))
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        break;
    }
    case MemoryType::VAAPI: {
        _va_mem_info = get_va_info_from_buffer(buffer);
        if (_va_mem_info.va_display == nullptr)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaDisplay");
        if ((int)_va_mem_info.va_surface_id < 0)
            throw std::runtime_error("Failed to map GstBuffer: failed to get VaSurfaceId");
        break;
    }
    default:
        throw std::runtime_error("Unsupported memory type to map tensor data");
    }

    _width = tensor_caps.GetDimension(2);
    _height = tensor_caps.GetDimension(1);
    // TODO: Currently used in opencv preproc only. Actually tells that format is planar, and not always RGBP
    _format = FOURCC_RGBP;

    _mem_type = memory_type;
    _tensor_planes_num = planes_num;
    _tensor_caps = tensor_caps;
    _mapped_buffer = buffer;
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

VaMemInfo FrameData::GetVaMemInfo() const {
    return _va_mem_info;
}

guint FrameData::GetPlanesNum() const {
    if (IsVideoFrameMapped())
        return GST_VIDEO_FRAME_N_PLANES(&_mapped_frame);
    else if (IsTensorMapped())
        return _tensor_planes_num;

    return 0;
}

uint8_t *FrameData::GetPlane(guint index) const {
    if (index >= GetPlanesNum())
        return nullptr;

    if (IsVideoFrameMapped())
        return reinterpret_cast<uint8_t *>(GST_VIDEO_FRAME_PLANE_DATA(&_mapped_frame, index));

    if (IsTensorMapped()) {
        uint32_t plane_size = _tensor_caps.GetDimension(1) * _tensor_caps.GetDimension(2);
        return _tensor_map_info.data + index * plane_size;
    }

    return nullptr;
}

uint32_t FrameData::GetOffset(guint index) const {
    if (IsVideoFrameMapped() && index < GetPlanesNum())
        return GST_VIDEO_FRAME_PLANE_OFFSET(&_mapped_frame, index);

    // TODO: is it always correct for tensor?
    return 0;
}

uint32_t FrameData::GetStride(guint index) const {
    if (IsVideoFrameMapped() && index < GetPlanesNum())
        return GST_VIDEO_FRAME_PLANE_STRIDE(&_mapped_frame, index);

    // TODO: is it always correct for tensor?
    return 0;
}

uint32_t FrameData::GetSize() const {
    if (IsVideoFrameMapped())
        return GST_VIDEO_FRAME_SIZE(&_mapped_frame);
    if (IsTensorMapped())
        return _tensor_map_info.size;

    return 0;
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
