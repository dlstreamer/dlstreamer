/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/gst/buffer.h"

namespace dlstreamer {

class BufferMapperGSTToCPU : public BufferMapper {
  public:
    BufferMapperGSTToCPU() = default;
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto gst_src_buffer = std::dynamic_pointer_cast<GSTBuffer>(src_buffer);
        if (!gst_src_buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to GSTBuffer");
        return map(gst_src_buffer, mode);
    }
    CPUBufferPtr map(GSTBufferPtr src_buffer, AccessMode mode) {
        int map_flags = 0;
        if (static_cast<int>(mode) & static_cast<int>(AccessMode::READ))
            map_flags |= GST_MAP_READ;
        if (static_cast<int>(mode) & static_cast<int>(AccessMode::WRITE))
            map_flags |= GST_MAP_WRITE;

        if (src_buffer->video_info()) {
            // Add GST_VIDEO_FRAME_MAP_FLAG_NO_REF to not increment/decrement GstBuffer ref-count during map/unmap
            map_flags |= GST_VIDEO_FRAME_MAP_FLAG_NO_REF;
            return mapVideoBuffer(src_buffer, (GstMapFlags)map_flags);
        } else {
            return mapGenericBuffer(src_buffer, (GstMapFlags)map_flags);
        }
    }

  private:
    CPUBufferPtr mapVideoBuffer(GSTBufferPtr src_buffer, GstMapFlags map_flags) {
        auto frame_ptr = std::make_shared<GstVideoFrame>();
        GstVideoFrame *frame = frame_ptr.get();
        if (!gst_video_frame_map(frame, (GstVideoInfo *)src_buffer->video_info(), src_buffer->gst_buffer(),
                                 map_flags)) {
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        }

        auto n_planes = GST_VIDEO_FRAME_N_PLANES(frame);
        std::vector<void *> data(n_planes);
        for (guint i = 0; i < n_planes; ++i) {
            data[i] = GST_VIDEO_FRAME_PLANE_DATA(frame, i);
        }
        // dst->_info->planes[i].stride = GST_VIDEO_FRAME_PLANE_STRIDE(frame, i);

        // get GstVideoInfo from GstVideoFrame, after gst_video_frame_map, video frame has valid offset and strides
        // obtained from GstVideMeta of GstBuffer
        auto dst = new CPUBuffer(gst_video_info_to_buffer_info(&frame->info), data);

#ifdef ENABLE_VPUX // also get DMA FD
        GstMemory *mem = gst_buffer_peek_memory(src_buffer->gst_buffer(), 0);
        if (!mem)
            throw std::runtime_error("Failed to get GstBuffer memory");
        if (gst_is_dmabuf_memory(mem)) {
            int dma_fd = gst_dmabuf_memory_get_fd(mem.get());
            set_handle("dma_fd", dma_fd);
        }
#endif
        auto deleter = [src_buffer, frame_ptr](CPUBuffer *dst) {
            gst_video_frame_unmap(frame_ptr.get());
            delete dst;
        };
        return CPUBufferPtr(dst, deleter);
    }

    CPUBufferPtr mapGenericBuffer(GSTBufferPtr src_buffer, GstMapFlags map_flags) {
        GstBuffer *gst_buffer = src_buffer->gst_buffer();
        guint n_planes = gst_buffer_n_memory(gst_buffer);

        auto map_info = new GstMapInfo[n_planes];
        std::vector<void *> data(n_planes);
        for (guint i = 0; i < n_planes; ++i) {
            GstMemory *mem = gst_buffer_peek_memory(gst_buffer, i);
            if (!mem || !gst_memory_map(mem, &map_info[i], map_flags))
                throw std::runtime_error("BufferMapperGSTToCPU: failed to map GstBuffer");
            data[i] = map_info[i].data;
        }
        auto dst = new CPUBuffer(src_buffer->info(), data);

        auto deleter = [src_buffer, map_info](CPUBuffer *dst) {
            GstBuffer *gst_buffer = src_buffer->gst_buffer();
            guint n_planes = gst_buffer_n_memory(gst_buffer);
            for (guint i = 0; i < n_planes; ++i) {
                GstMemory *mem = gst_buffer_peek_memory(gst_buffer, i);
                gst_memory_unmap(mem, &map_info[i]);
            }
            delete[] map_info;
            delete dst;
        };
        return CPUBufferPtr(dst, deleter);
    }
};

} // namespace dlstreamer
