/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_buffer_map.h"
#include "inference_backend/logger.h"
#ifndef DISABLE_VAAPI
#include "gst_vaapi_plugins_utils.h"
#endif
#include <gst/allocators/allocators.h>

#define VA_CALL(_FUNC)                                                                                                 \
    {                                                                                                                  \
        ITT_TASK(#_FUNC);                                                                                              \
        VAStatus _status = _FUNC;                                                                                      \
        if (_status != VA_STATUS_SUCCESS) {                                                                            \
            GST_ERROR(#_FUNC " failed, sts=%d: %s", _status, vaErrorStr(_status));                                     \
        }                                                                                                              \
    }

inline int gstFormatToFourCC(int format) {
    switch (format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG("GST_VIDEO_FORMAT_NV12");
        return InferenceBackend::FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_BGRx:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRx");
        return InferenceBackend::FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRA");
        return InferenceBackend::FourCC::FOURCC_BGRA;
#if VA_MAJOR_VERSION >= 1
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return InferenceBackend::FourCC::FOURCC_I420;
#endif
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
    return 0;
}

bool gva_buffer_map(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &mapContext, GstVideoInfo *info,
                    InferenceBackend::MemoryType memoryType) {
    image = {};
    mapContext = {};

    image.format = gstFormatToFourCC(info->finfo->format);
    image.width = static_cast<int>(info->width);
    image.height = static_cast<int>(info->height);
    for (int i = 0; i < 4; i++) {
        image.stride[i] = info->stride[i];
    }

#ifndef DISABLE_VAAPI
    GstMemory *mem = gst_buffer_get_memory(buffer, 0);
    if (mem && gst_memory_is_type(mem, "GstVaapiVideoMemory")) {
        image.type = InferenceBackend::MemoryType::VAAPI;
        GstVaapi_GetDisplayAndSurface(buffer, image.va_display, image.va_surface);

        if (memoryType == InferenceBackend::MemoryType::SYSTEM) {
            void *surface_p;
            mapContext.va_display = image.va_display;
            VA_CALL(vaDeriveImage(image.va_display, image.va_surface, &mapContext.va_image));
            VA_CALL(vaMapBuffer(image.va_display, mapContext.va_image.buf, &surface_p));
            for (int i = 0; i < 4; i++) {
                image.planes[i] = (uint8_t *)surface_p + info->offset[i];
            }
            image.type = InferenceBackend::MemoryType::SYSTEM;
        }
    } else
#endif
    {
        if (!gst_buffer_map(buffer, &mapContext.gstMapInfo, GST_MAP_READ)) {
            GST_ERROR("gva_buffer_map: gst_buffer_map failed\n");
        }
        for (int i = 0; i < 4; i++) {
            image.planes[i] = mapContext.gstMapInfo.data + info->offset[i];
        }
    }
    return true;
}

void gva_buffer_unmap(GstBuffer *buffer, InferenceBackend::Image &, BufferMapContext &mapContext) {
    if (mapContext.gstMapInfo.size) {
        gst_buffer_unmap(buffer, &mapContext.gstMapInfo);
    }
#ifndef DISABLE_VAAPI
    if (mapContext.va_display) {
        VA_CALL(vaUnmapBuffer(mapContext.va_display, mapContext.va_image.buf));
        VA_CALL(vaDestroyImage(mapContext.va_display, mapContext.va_image.image_id));
    }
#endif
}
