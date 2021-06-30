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

struct VaMemInfo {
    uint32_t va_surface_id = 0;
    void *va_display = nullptr;
};

VaMemInfo get_va_info_from_buffer(GstBuffer *buffer);

// TODO: rename?
// TODO: Contains video specific info, should be more abstract or not?
// TODO: split system, dma, vaapi
class FrameData {
  public:
    FrameData();
    ~FrameData();

    void Map(GstBuffer *buffer, GstVideoInfo *video_info, InferenceBackend::MemoryType memory_type,
             GstMapFlags map_flags);
    void Map(GstBuffer *buffer, const TensorCaps &tensor_caps, GstMapFlags map_flags, int planes_num = 3,
             InferenceBackend::MemoryType memory_type = InferenceBackend::MemoryType::SYSTEM);

    bool IsMapped() const;
    void Unmap();

    InferenceBackend::MemoryType GetMemoryType() const;
    int GetDMABufDescriptor() const;
    VaMemInfo GetVaMemInfo() const;

    guint GetPlanesNum() const;
    uint8_t *GetPlane(guint index) const;
    uint32_t GetOffset(guint index) const;
    uint32_t GetStride(guint index) const;
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
    GstVideoFrame _mapped_frame;
    GstMapInfo _tensor_map_info;
    GstBuffer *_mapped_buffer;
    int _tensor_planes_num = 0;
    int _dma_fd = -1;
    VaMemInfo _va_mem_info;
    uint32_t _width = 0;
    uint32_t _height = 0;
    InferenceBackend::FourCC _format;
};
