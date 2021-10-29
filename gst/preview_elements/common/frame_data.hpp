/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "capabilities/types.hpp"
#include "memory_type.hpp"

#include <gst/video/video.h>

#include <cstdint>

constexpr auto MAX_PLANES_NUM = 4;
const uint32_t VASURFACE_INVALID_ID = 0xffffffff;

// TODO: rename?
// TODO: Contains video specific info, should be more abstract or not?
// TODO: split system, dma, vaapi
class FrameData {
  public:
    FrameData();
    ~FrameData();

    void Map(GstBuffer *buffer, GstVideoInfo *video_info, InferenceBackend::MemoryType memory_type,
             GstMapFlags map_flags);
    void Map(GstBuffer *buffer, const TensorCaps &tensor_caps, GstMapFlags map_flags,
             InferenceBackend::MemoryType memory_type = InferenceBackend::MemoryType::SYSTEM, uint32_t planes_num = 1,
             std::vector<size_t> plane_sizes = {});

    bool IsMapped() const;
    void Unmap();

    InferenceBackend::MemoryType GetMemoryType() const;
    int GetDMABufDescriptor() const;
    uint32_t GetVaSurfaceID() const;

    uint32_t GetPlanesNum() const;
    uint8_t *GetPlane(uint32_t index) const;
    uint32_t GetOffset(uint32_t index) const;
    uint32_t GetStride(uint32_t index) const;

    const std::vector<uint8_t *> &GetPlanes() const;
    const std::vector<uint32_t> &GetStrides() const;
    const std::vector<uint32_t> &GetOffsets() const;

    uint32_t GetSize() const;
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    InferenceBackend::FourCC GetFormat() const;
    Precision GetPrecision() const;
    Layout GetLayout() const;

  private:
    bool IsVideoFrameMapped() const;
    bool IsTensorMapped() const;

  private:
    InferenceBackend::MemoryType _mem_type = InferenceBackend::MemoryType::SYSTEM;
    TensorCaps _tensor_caps;
    GstVideoFrame _mapped_frame = {};
    GstMapInfo _tensor_map_info = {};
    GstBuffer *_mapped_buffer = nullptr;
    int _dma_fd = -1;
    uint32_t _va_surface_id = VASURFACE_INVALID_ID;
    uint32_t _width = 0;
    uint32_t _height = 0;
    InferenceBackend::FourCC _format = InferenceBackend::FourCC::FOURCC_BGR;

    uint32_t _size = 0;
    std::vector<uint8_t *> _planes;
    std::vector<uint32_t> _strides;
    std::vector<uint32_t> _offsets;
};
