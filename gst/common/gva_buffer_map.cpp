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
int gva_dmabuffer_import(GstMemory *mem, unsigned int vpu_device_id) {
    int fd = 0;
    try {
        fd = gst_dmabuf_memory_get_fd(mem);
        if (fd <= 0)
            throw std::runtime_error("Failed to get file desc associated with GstBuffer memory");

        long int phyAddr = vpurm_import_dmabuf(fd, VPU_DEFAULT, vpu_device_id);
        if (phyAddr <= 0)
            throw std::runtime_error("Failed to import DMA buffer from file desc");
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to import DMA buffer memory from GstBuffer"));
    }
    return fd;
}

void gva_dmabuffer_unimport(GstMemory *mem, unsigned int vpu_device_id) {
    int fd = 0;
    try {
        fd = gst_dmabuf_memory_get_fd(mem);
        if (fd <= 0)
            throw std::runtime_error("Failed to get file desc associated with GstBuffer memory");

        vpurm_unimport_dmabuf(fd, vpu_device_id);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to unimport DMA buffer memory from GstBuffer"));
    }
}
#endif

void gva_buffer_map(GstBuffer *buffer, Image &image, BufferMapContext &map_context, GstVideoInfo *info,
                    MemoryType memory_type, GstMapFlags map_flags, unsigned int vpu_device_id) {
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
        image.width = static_cast<uint32_t>(GST_VIDEO_INFO_WIDTH(info));
        image.height = static_cast<uint32_t>(GST_VIDEO_INFO_HEIGHT(info));
        image.size = GST_VIDEO_INFO_SIZE(info);
        image.type = memory_type;
        for (guint i = 0; i < n_planes; ++i) {
            image.stride[i] = GST_VIDEO_INFO_PLANE_STRIDE(info, i);
            image.offsets[i] = GST_VIDEO_INFO_PLANE_OFFSET(info, i);
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
#if defined(USE_VPUSMM)
            auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
            if (not mem.get())
                throw std::runtime_error("Failed to get GstBuffer memory");
            if (gst_is_dmabuf_memory(mem.get()))
                gva_dmabuffer_import(mem.get(), vpu_device_id);
#else
            UNUSED(vpu_device_id);
#endif
            break;
        }
        case MemoryType::DMA_BUFFER: {
            GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
            if (not mem)
                throw std::runtime_error("Failed to get GstBuffer memory");
            image.dma_fd = gst_dmabuf_memory_get_fd(mem);
            if (image.dma_fd < 0)
                throw std::runtime_error("Failed to import DMA buffer FD");
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

void gva_buffer_unmap(GstBuffer *buffer, Image &, BufferMapContext &map_context, unsigned int vpu_device_id) {
    if (map_context.frame.buffer) {
#if defined(USE_VPUSMM)
        auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
        if (not mem.get())
            throw std::runtime_error("Failed to get GstBuffer memory");
        if (gst_is_dmabuf_memory(mem.get()))
            gva_dmabuffer_unimport(mem.get(), vpu_device_id);
#else
        UNUSED(buffer);
        UNUSED(vpu_device_id);
#endif
        gst_video_frame_unmap(&map_context.frame);
    }
}
