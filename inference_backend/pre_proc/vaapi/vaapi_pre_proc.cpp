/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <memory.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>

#include "vaapi_pre_proc.h"
#include "vaapi_utils.h"

namespace InferenceBackend {

PreProc *CreatePreProcVAAPI() {
    return new VAAPIPreProc();
}

VAAPIPreProc::~VAAPIPreProc() {
    if (va_context != VA_INVALID_ID) {
        vaDestroyContext(va_display, va_context);
    }
    if (va_config != VA_INVALID_ID) {
        vaDestroyContext(va_display, va_config);
    }
    if (va_display) {
        vaTerminate(va_display);
    }
}

static uint32_t Fourcc2RTFormat(int format_fourcc) {
    switch (format_fourcc) {
#if VA_MAJOR_VERSION >= 1
    case VA_FOURCC_I420:
        return VA_FOURCC_I420;
#endif
    case VA_FOURCC_NV12:
        return VA_RT_FORMAT_YUV420;
    case VA_FOURCC_RGBP:
        return VA_RT_FORMAT_RGBP;
    default:
        return VA_RT_FORMAT_RGB32;
    }
}

VAAPIPreProc::VAAPIPreProc() {
    int fd = open("/dev/dri/renderD128", O_RDWR);
    if (!fd) {
        throw std::runtime_error("Error opening VAAPI device");
    }

    va_display = vaGetDisplayDRM(fd);
    if (!va_display) {
        throw std::runtime_error("Error opening VAAPI display");
    }

    int major_version = 0, minor_version = 0;
    VA_CALL(vaInitialize(va_display, &major_version, &minor_version))

    VA_CALL(vaCreateConfig(va_display, VAProfileNone, VAEntrypointVideoProc, nullptr, 0, &va_config))

    VA_CALL(vaCreateContext(va_display, va_config, 0, 0, VA_PROGRESSIVE, 0, 0, &va_context))
}

static VASurfaceID CreateVASurface(VADisplay dpy, const Image &src) {
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = src.format;

    unsigned int rtformat = Fourcc2RTFormat(src.format);

    VASurfaceID va_surface_id;
    VA_CALL(vaCreateSurfaces(dpy, rtformat, src.width, src.height, &va_surface_id, 1, &surface_attrib, 1))
    return va_surface_id;
}

static VASurfaceID CreateVASurfaceFromDMA(VADisplay vpy, const Image &src) {
    if (src.type != InferenceBackend::MemoryType::DMA_BUFFER) {
        throw std::runtime_error("MemoryType=DMA_BUFFER expected");
    }

    VASurfaceAttrib attribs[2] = {};
    VASurfaceAttribExternalBuffers external = {};
    VASurfaceID va_surface_id;

    external.width = src.width;
    external.height = src.height;
    external.num_planes = GetPlanesCount(src.format);
    uint64_t dma_fd = src.dma_fd;
    external.buffers = &dma_fd;
    external.num_buffers = 1;
    external.pixel_format = src.format;
    external.data_size = 0;
    for (uint32_t i = 0; i < external.num_planes; i++) {
        external.pitches[i] = src.stride[i];
        external.data_size += src.stride[i] * src.height;
    }

    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    VA_CALL(vaCreateSurfaces(vpy, Fourcc2RTFormat(src.format), src.width, src.height, &va_surface_id, 1, attribs, 2))

    return va_surface_id;
}

static VASurfaceID CreateVASurfaceFromAlignedBuffer(VADisplay dpy, Image &src) {
    if (src.type != InferenceBackend::MemoryType::SYSTEM) {
        throw std::runtime_error("MemoryType=SYSTEM expected");
    }

    VASurfaceAttribExternalBuffers external{};
    external.pixel_format = src.format;
    external.width = src.width;
    external.height = src.height;
    uintptr_t buffers[1] = {(uintptr_t)src.planes[0]};
    external.num_buffers = 1;
    external.buffers = buffers;
    external.flags = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
    external.num_planes = GetPlanesCount(src.format);
    for (int i = 0; i < external.num_planes; i++) {
        external.pitches[i] = src.stride[i];
        external.offsets[i] = src.planes[i] - src.planes[0];
        external.data_size += src.stride[i] * src.height;
    }

    VASurfaceAttrib attribs[2]{};
    attribs[0].type = (VASurfaceAttribType)VASurfaceAttribMemoryType;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

    attribs[1].type = (VASurfaceAttribType)VASurfaceAttribExternalBufferDescriptor;
    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = (void *)&external;

    VASurfaceID va_surface_id;
    VA_CALL(vaCreateSurfaces(dpy, Fourcc2RTFormat(src.format), src.width, src.height, &va_surface_id, 1, attribs, 2))

    return va_surface_id;
}

void VAAPIPreProc::Convert(const Image &src, Image &dst, bool /*bAllocateDestination*/) {
    VASurfaceID src_surface = CreateVASurfaceFromDMA(va_display, src);

    if (dst.type == MemoryType::ANY) {
        dst.va_surface_id = CreateVASurface(va_display, dst);
        dst.va_display = va_display;
        dst.type = MemoryType::VAAPI;
    }
    VASurfaceID dst_surface = dst.va_surface_id;

    VAProcPipelineParameterBuffer pipeline_param = {};
    pipeline_param.surface = src_surface;
    VARectangle surface_region = {.x = (int16_t)src.rect.x,
                                  .y = (int16_t)src.rect.y,
                                  .width = (uint16_t)src.rect.width,
                                  .height = (uint16_t)src.rect.height};
    if (surface_region.width > 0 && surface_region.height > 0)
        pipeline_param.surface_region = &surface_region;

    // pipeline_param.filter_flags = VA_FILTER_SCALING_HQ; // High-quality scaling method

    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VA_CALL(vaCreateBuffer(va_display, va_context, VAProcPipelineParameterBufferType, sizeof(pipeline_param), 1,
                           &pipeline_param, &pipeline_param_buf_id));

    VA_CALL(vaBeginPicture(va_display, va_context, dst_surface))

    VA_CALL(vaRenderPicture(va_display, va_context, &pipeline_param_buf_id, 1))

    VA_CALL(vaEndPicture(va_display, va_context))

    VA_CALL(vaDestroyBuffer(va_display, pipeline_param_buf_id))

    VA_CALL(vaDestroySurfaces(va_display, &src_surface, 1))
}

void VAAPIPreProc::ReleaseImage(const Image &image) {
    if (image.type == MemoryType::VAAPI && image.va_surface_id && image.va_surface_id != VA_INVALID_ID) {
        VA_CALL(vaDestroySurfaces(va_display, (uint32_t *)&image.va_surface_id, 1))
        image.type == MemoryType::ANY;
    }
}

} // namespace InferenceBackend
