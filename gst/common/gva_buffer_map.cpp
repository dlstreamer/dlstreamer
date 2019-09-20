/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_buffer_map.h"
#include <gst/allocators/allocators.h>

using namespace InferenceBackend;

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
#if VA_MAJOR_VERSION >= 1
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return FourCC::FOURCC_I420;
#endif
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
    return 0;
}

bool gva_buffer_map(GstBuffer *buffer, Image &image, BufferMapContext &map_context, GstVideoInfo *info,
                    MemoryType memory_type, GstMapFlags map_flags) {
    image = {};
    map_context = {};
    bool status = true;

    guint n_planes = info->finfo->n_planes;
    if (n_planes > Image::MAX_PLANES_NUMBER)
        throw std::logic_error("Planes number " + std::to_string(n_planes) + " isn't supported");

    image.format = gstFormatToFourCC(info->finfo->format);
    image.width = static_cast<int>(info->width);
    image.height = static_cast<int>(info->height);
    for (guint i = 0; i < n_planes; i++) {
        image.stride[i] = info->stride[i];
    }

    image.type = memory_type;
    switch (memory_type) {
    case MemoryType::SYSTEM:
        if (!gst_buffer_map(buffer, &map_context.gstMapInfo, map_flags)) {
            status = false;
            GST_ERROR("gva_buffer_map: gst_buffer_map failed");
            break;
        }
        for (guint i = 0; i < n_planes; i++)
            image.planes[i] = map_context.gstMapInfo.data + info->offset[i];
        break;
    case MemoryType::DMA_BUFFER: {
        GstMemory *mem = gst_buffer_get_memory(buffer, 0);
        image.dma_fd = gst_fd_memory_get_fd(mem);
        gst_memory_unref(mem);
        if (!image.dma_fd) {
            status = false;
            GST_ERROR("gva_buffer_map: gst_fd_memory_get_fd failed");
        }
        break;
    }
    case MemoryType::VAAPI:
        image.va_display = gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VADisplay"));
        image.va_surface_id =
            (uint64_t)gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VASurfaceID"));
        if (!image.va_display) {
            status = false;
            GST_ERROR("gva_buffer_map: failed to get VADisplay=%p", image.va_display);
        }
        if ((int)image.va_surface_id < 0) {
            status = false;
            GST_ERROR("gva_buffer_map: failed to get VASurfaceID=%d", image.va_surface_id);
        }
        break;
    default:
        GST_ERROR("gva_buffer_map: unsupported memory type");
        status = false;
        break;
    }

    return status;
}

void gva_buffer_unmap(GstBuffer *buffer, Image &, BufferMapContext &map_context) {
    if (map_context.gstMapInfo.size) {
        gst_buffer_unmap(buffer, &map_context.gstMapInfo);
    }
}
