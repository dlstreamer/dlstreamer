/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_buffer_map.h"
#include <gst/allocators/allocators.h>
#include <gst/video/video-frame.h>
#include <memory>
#include <sstream>
#include <stdexcept>

#define UNUSED(x) (void)(x)

#ifdef USE_VPUSMM
#include <vpusmm/vpusmm.h>
#endif
#include <inference_backend/logger.h>

using namespace InferenceBackend;
using GstMemoryUniquePtr = std::unique_ptr<GstMemory, decltype(&gst_memory_unref)>;

inline int gstFormatToFourCC(int format) {
    switch (format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG("GST_VIDEO_FORMAT_NV12");
        return FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_BGR:
        GST_DEBUG("GST_VIDEO_FORMAT_BGR");
        return FourCC::FOURCC_BGR;
    case GST_VIDEO_FORMAT_BGRx:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRx");
        return FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRA");
        return FourCC::FOURCC_BGRA;
    case GST_VIDEO_FORMAT_RGBA:
        GST_DEBUG("GST_VIDEO_FORMAT_RGBA");
        return FourCC::FOURCC_RGBA;
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return FourCC::FOURCC_I420;
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
    return 0;
}

#ifdef USE_VPUSMM
int gva_dmabuffer_import(GstBuffer *buffer) {
    int fd = 0;
    auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
    try {
        if (not mem.get())
            throw std::runtime_error("Failed to get GstBuffer memory");

        if (not gst_is_dmabuf_memory(mem.get()))
            throw std::runtime_error("GstBuffer memory is not DMA buffer");

        int fd = gst_dmabuf_memory_get_fd(mem.get());
        if (fd <= 0)
            throw std::runtime_error("Failed to get file desc associated with GstBuffer memory");

        long int phyAddr = vpusmm_import_dmabuf(fd, VPU_DEFAULT);
        if (phyAddr <= 0)
            throw std::runtime_error("Failed to import DMA buffer from file desc");
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to import DMA buffer memory from GstBuffer"));
    }
    return fd;
}

void gva_dmabuffer_unimport(GstBuffer *buffer) {
    auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
    try {
        if (not mem.get())
            throw std::runtime_error("Failed to get GstBuffer memory");

        if (not gst_is_dmabuf_memory(mem.get()))
            throw std::runtime_error("GstBuffer memory is not DMA buffer");

        int fd = gst_dmabuf_memory_get_fd(mem.get());
        if (fd <= 0)
            throw std::runtime_error("Failed to get file desc associated with GstBuffer memory");

        vpusmm_unimport_dmabuf(fd);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to unimport DMA buffer memory from GstBuffer"));
    }
}
#endif

void gva_buffer_map(GstBuffer *buffer, Image &image, BufferMapContext &map_context, GstVideoInfo *info,
                    MemoryType memory_type, GstMapFlags map_flags) {
    ITT_TASK(__FUNCTION__);
    try {
        if (not info)
            throw std::invalid_argument("GstVideoInfo is absent during GstBuffer mapping");
        image = Image();
        map_context = BufferMapContext();
        map_context.frame.buffer = nullptr;
        guint n_planes = GST_VIDEO_INFO_N_PLANES(info);
        if (n_planes == 0 or n_planes > Image::MAX_PLANES_NUMBER)
            throw std::logic_error("Image planes number " + std::to_string(n_planes) + " isn't supported");

        image.format = gstFormatToFourCC(GST_VIDEO_INFO_FORMAT(info));
        image.width = static_cast<int>(GST_VIDEO_INFO_WIDTH(info));
        image.height = static_cast<int>(GST_VIDEO_INFO_HEIGHT(info));
        image.type = memory_type;
        for (guint i = 0; i < n_planes; ++i) {
            image.stride[i] = GST_VIDEO_INFO_PLANE_STRIDE(info, i);
        }

        switch (memory_type) {
        case MemoryType::SYSTEM: {
            if (not gst_video_frame_map(&map_context.frame, info, buffer, map_flags)) {
                throw std::runtime_error("Failed to map GstBuffer to system memory");
            }
            for (guint i = 0; i < n_planes; ++i) {
                image.planes[i] = static_cast<uint8_t *>(GST_VIDEO_FRAME_PLANE_DATA(&map_context.frame, i));
            }
            for (guint i = 0; i < n_planes; ++i) {
                image.stride[i] = GST_VIDEO_FRAME_PLANE_STRIDE(&map_context.frame, i);
            }
#ifdef USE_VPUSMM
            gva_dmabuffer_import(buffer);
#endif
            break;
        }
        case MemoryType::DMA_BUFFER: {
            auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
            if (not mem.get())
                throw std::runtime_error("Failed to get GstBuffer memory");
            image.dma_fd = gst_fd_memory_get_fd(mem.get()); // gst_dmabuf_memory_get_fd?
            if (image.dma_fd <= 0)
                throw std::runtime_error("Failed to import DMA buffer memory");
            break;
        }
        case MemoryType::VAAPI:
            image.va_display = gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VADisplay"));
            image.va_surface_id =
                (uint64_t)gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VASurfaceID"));
            if (not image.va_display) {
                std::ostringstream os;
                os << "Failed to get VADisplay=" << image.va_display;
                throw std::runtime_error(os.str());
            }
            if ((int)image.va_surface_id < 0) {
                std::ostringstream os;
                os << "Failed to get VASurfaceID=" << image.va_surface_id;
                throw std::runtime_error(os.str());
            }
            break;
        default:
            throw std::logic_error("Unsupported destination memory type");
        }
    } catch (const std::exception &e) {
        image = Image();
        map_context.frame.buffer = nullptr;
        std::throw_with_nested(std::runtime_error("Failed to map GstBuffer to specific memory type"));
    }
}

void gva_buffer_unmap(GstBuffer *buffer, Image &, BufferMapContext &map_context) {
    if (map_context.frame.buffer) {
#if defined(USE_VPUSMM)
        gva_dmabuffer_unimport(buffer);
#else
        UNUSED(buffer);
#endif
        gst_video_frame_unmap(&map_context.frame);
    }
}
