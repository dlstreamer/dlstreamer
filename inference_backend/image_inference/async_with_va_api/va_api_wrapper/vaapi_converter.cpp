/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_converter.h"

#include <stdexcept>
#include <string>
#include <unistd.h>

#include "inference_backend/pre_proc.h"
#include "inference_backend/safe_arithmetic.h"

#include <va/va_drmcommon.h>

using namespace InferenceBackend;

namespace {

VASurfaceID ConvertVASurfaceFromDifferentDriverContext(VaDpyWrapper display1, VASurfaceID surface,
                                                       VaDpyWrapper display2, int rt_format, uint64_t &dma_fd_out) {

    VADRMPRIMESurfaceDescriptor drm_descriptor = VADRMPRIMESurfaceDescriptor();
    VA_CALL(display2.drvVtable().vaExportSurfaceHandle(display2.drvCtx(), surface,
                                                       VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                                       VA_EXPORT_SURFACE_READ_ONLY, &drm_descriptor));

    VASurfaceAttribExternalBuffers external = VASurfaceAttribExternalBuffers();
    external.width = drm_descriptor.width;
    external.height = drm_descriptor.height;
    external.pixel_format = drm_descriptor.fourcc;

    if (drm_descriptor.num_objects != 1)
        throw std::invalid_argument("Unexpected objects number");
    auto object = drm_descriptor.objects[0];
    external.num_buffers = 1;
    uint64_t dma_fd = object.fd;
    dma_fd_out = dma_fd;
    external.buffers = &dma_fd;
    external.data_size = object.size;

    external.num_planes = drm_descriptor.num_layers;
    for (uint32_t i = 0; i < external.num_planes; i++) {
        auto layer = drm_descriptor.layers[i];
        if (layer.num_planes != 1)
            throw std::invalid_argument("Unexpected planes number");
        external.pitches[i] = layer.pitch[0];
        external.offsets[i] = layer.offset[0];
    }

    VASurfaceAttrib attribs[2] = {};
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    VASurfaceID va_surface_id = VA_INVALID_SURFACE;
    VA_CALL(display1.drvVtable().vaCreateSurfaces2(display1.drvCtx(), rt_format, drm_descriptor.width,
                                                   drm_descriptor.height, &va_surface_id, 1, attribs, 2));
    return va_surface_id;
}

VASurfaceID ConvertDMABuf(VaDpyWrapper display, const Image &src, int rt_format) {
    if (src.type != MemoryType::DMA_BUFFER) {
        throw std::runtime_error("MemoryType=DMA_BUFFER expected");
    }

    VASurfaceAttribExternalBuffers external = VASurfaceAttribExternalBuffers();
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
        external.offsets[i] = src.offsets[i];
    }
    external.data_size = src.size;

    VASurfaceAttrib attribs[2] = {};
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].type = VASurfaceAttribMemoryType;
    attribs[0].value.type = VAGenericValueTypeInteger;
    attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = &external;

    VASurfaceID va_surface_id;
    VA_CALL(display.drvVtable().vaCreateSurfaces2(display.drvCtx(), rt_format, src.width, src.height, &va_surface_id, 1,
                                                  attribs, 2))

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
    VA_CALL(vaCreateSurfaces(dpy, FourCc2RTFormat(src.format), src.width, src.height, &va_surface_id, 1, attribs,
2))

    return va_surface_id;
}*/

} // anonymous namespace

VaApiConverter::VaApiConverter(VaApiContext *context) : _context(context) {
    if (!context)
        throw std::runtime_error("VaApiContext is null. VaConverter requers not nullptr context.");
}

void VaApiConverter::Convert(const Image &src, VaApiImage &va_api_dst) {
    VASurfaceID src_surface = VA_INVALID_SURFACE;
    Image &dst = va_api_dst.image;

    uint64_t dma_fd = 0;

    if (src.type == MemoryType::VAAPI) {
        src_surface = ConvertVASurfaceFromDifferentDriverContext(_context->Display(), src.va_surface_id,
                                                                 VaDpyWrapper::fromHandle(src.va_display),
                                                                 _context->RTFormat(), dma_fd);
    } else if (src.type == MemoryType::DMA_BUFFER) {
        src_surface = ConvertDMABuf(_context->Display(), src, _context->RTFormat());
    } else {
        throw std::runtime_error("VaApiConverter::Convert: unsupported MemoryType");
    }

    VASurfaceID dst_surface = dst.va_surface_id;

    VAProcPipelineParameterBuffer pipeline_param = VAProcPipelineParameterBuffer();
    pipeline_param.surface = src_surface;
    VARectangle surface_region = {.x = static_cast<int16_t>(src.rect.x),
                                  .y = static_cast<int16_t>(src.rect.y),
                                  .width = static_cast<uint16_t>(src.rect.width),
                                  .height = static_cast<uint16_t>(src.rect.height)};
    if (surface_region.width > 0 && surface_region.height > 0)
        pipeline_param.surface_region = &surface_region;

    // pipeline_param.filter_flags = VA_FILTER_SCALING_HQ; // High-quality scaling method

    auto context = _context->Display().drvCtx();
    auto vtable = _context->Display().drvVtable();
    VABufferID pipeline_param_buf_id = VA_INVALID_ID;
    VA_CALL(vtable.vaCreateBuffer(context, _context->Id(), VAProcPipelineParameterBufferType, sizeof(pipeline_param), 1,
                                  &pipeline_param, &pipeline_param_buf_id));

    VA_CALL(vtable.vaBeginPicture(context, _context->Id(), dst_surface))

    VA_CALL(vtable.vaRenderPicture(context, _context->Id(), &pipeline_param_buf_id, 1))

    VA_CALL(vtable.vaEndPicture(context, _context->Id()))

    VA_CALL(vtable.vaDestroyBuffer(context, pipeline_param_buf_id))

    VA_CALL(vtable.vaDestroySurfaces(context, &src_surface, 1))

    if (src.type == MemoryType::VAAPI)
        if (close(dma_fd) == -1)
            throw std::runtime_error("VaApiConverter::Convert: close fd failed");
}
