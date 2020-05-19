/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <stdexcept>
#include <string>
#include <va/va.h>
#include <va/va_drmcommon.h>

#include "inference_backend/pre_proc.h"
#include "inference_backend/safe_arithmetic.h"
#include "vaapi_converter.h"
#include "vaapi_images.h"
#include "vaapi_utils.h"

using namespace InferenceBackend;

namespace {

uint32_t FourCc2RTFormat(int format_four_cc) {
    switch (format_four_cc) {
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

VASurfaceID CreateVASurface(VADisplay dpy, const Image &src) {
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = src.format;

    unsigned int rt_format = FourCc2RTFormat(src.format);

    VASurfaceID va_surface_id;
    VA_CALL(vaCreateSurfaces(dpy, rt_format, src.width, src.height, &va_surface_id, 1, &surface_attrib, 1))
    return va_surface_id;
}

VASurfaceID CreateVASurfaceFromDMA(VADisplay vpy, const Image &src) {
    if (src.type != InferenceBackend::MemoryType::DMA_BUFFER) {
        throw std::runtime_error("MemoryType=DMA_BUFFER expected");
    }

    VASurfaceAttrib attribs[2] = {};
    VASurfaceAttribExternalBuffers external = VASurfaceAttribExternalBuffers();
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
        external.data_size = safe_add(external.data_size, safe_mul(src.stride[i], src.height));
    }

    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    VA_CALL(vaCreateSurfaces(vpy, FourCc2RTFormat(src.format), src.width, src.height, &va_surface_id, 1, attribs, 2))

    return va_surface_id;
}

/* static VASurfaceID CreateVASurfaceFromAlignedBuffer(VADisplay dpy, Image &src) {
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
    for (uint32_t i = 0; i < external.num_planes; i++) {
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
    VA_CALL(vaCreateSurfaces(dpy, FourCc2RTFormat(src.format), src.width, src.height, &va_surface_id, 1, attribs, 2))

    return va_surface_id;
}*/

} // anonymous namespace

VaApiConverter::VaApiConverter(VaApiContext *context) : _context(context) {
}

void VaApiConverter::Convert(const Image &src, VaApiImage &va_api_dst) {
    VASurfaceID src_surface;
    Image &dst = va_api_dst.image;

    if (src.type == MemoryType::VAAPI) {
        src_surface = src.va_surface_id;
    } else if (src.type == MemoryType::DMA_BUFFER) {
        src_surface = CreateVASurfaceFromDMA(_context->Display(), src);
    } else {
        throw std::runtime_error("VaApiConverter::Convert: unsupported MemoryType");
    }

    if (dst.type == MemoryType::ANY) {
        dst.va_surface_id = CreateVASurface(_context->Display(), dst);
        dst.va_display = _context->Display();
        dst.type = MemoryType::VAAPI;
    }
    VASurfaceID dst_surface = dst.va_surface_id;

    VAProcPipelineParameterBuffer pipeline_param = VAProcPipelineParameterBuffer();
    pipeline_param.surface = src_surface;
    VARectangle surface_region = {.x = safe_convert<int16_t>(src.rect.x),
                                  .y = safe_convert<int16_t>(src.rect.y),
                                  .width = safe_convert<uint16_t>(src.rect.width),
                                  .height = safe_convert<uint16_t>(src.rect.height)};
    if (surface_region.width > 0 && surface_region.height > 0)
        pipeline_param.surface_region = &surface_region;

    // pipeline_param.filter_flags = VA_FILTER_SCALING_HQ; // High-quality scaling method

    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VA_CALL(vaCreateBuffer(_context->Display(), _context->Id(), VAProcPipelineParameterBufferType,
                           sizeof(pipeline_param), 1, &pipeline_param, &pipeline_param_buf_id));

    VA_CALL(vaBeginPicture(_context->Display(), _context->Id(), dst_surface))

    VA_CALL(vaRenderPicture(_context->Display(), _context->Id(), &pipeline_param_buf_id, 1))

    VA_CALL(vaEndPicture(_context->Display(), _context->Id()))

    VA_CALL(vaDestroyBuffer(_context->Display(), pipeline_param_buf_id))

    if (src.type == MemoryType::DMA_BUFFER) {
        VA_CALL(vaDestroySurfaces(_context->Display(), &src_surface, 1))
    }
}
