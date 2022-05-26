/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/dma/buffer.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/utils.h"

#ifdef ENABLE_VAAPI
#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>
#endif

namespace dlstreamer {

#ifdef ENABLE_VAAPI

class BufferMapperDMAToVAAPI : public BufferMapper {
  public:
    BufferMapperDMAToVAAPI(ContextPtr context) {
        _vaapi_context = std::dynamic_pointer_cast<VAAPIContext>(context);
        if (!_vaapi_context)
            throw std::invalid_argument("Invalid context type provided: VAAPI context is expected");
    }

    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto dma_buffer = std::dynamic_pointer_cast<DMABuffer>(src_buffer);
        if (!dma_buffer)
            throw std::runtime_error("Invalid buffer type: DMA buffer is expected");
        return map(std::move(dma_buffer), mode);
    }

    VAAPIBufferPtr map(DMABufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        uint32_t fourcc = format_to_vaapi(info->format);
        uint32_t rtformat = vaapi_fourcc_to_rtformat(fourcc);

        // width and height from plane[0]
        VASurfaceAttribExternalBuffers external{};
        const PlaneInfo &plane = info->planes.front();
        external.width = plane.width();
        external.height = plane.height();
        if (plane.layout.n_position() >= 0) {
            external.height *= plane.batch();
        }
        external.num_planes = info->planes.size();

        uint64_t dma_fd = buffer->fd();
        external.buffers = &dma_fd;
        external.num_buffers = 1;
        external.pixel_format = fourcc;
        external.data_size = 0;
        for (size_t i = 0; i < external.num_planes; i++) {
            external.pitches[i] = info->planes[i].width_stride();
            external.offsets[i] = info->planes[i].offset;
            external.data_size += info->planes[i].size();
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
        auto drv_ctx = reinterpret_cast<VADisplayContextP>(_vaapi_context->va_display())->pDriverContext;
        VAStatus status = drv_ctx->vtable->vaCreateSurfaces2(drv_ctx, rtformat, external.width, external.height,
                                                             &va_surface_id, 1, attribs, 2);
        if (status != VA_STATUS_SUCCESS)
            throw std::runtime_error("Coulnd't create VASurface from DMA: vaCreateSurfaces2 failed, " +
                                     std::to_string(status));

        // Capture input DMABufferPtr in deleter
        auto deleter = [buffer, drv_ctx](VAAPIBuffer *buf) {
            auto surface = buf->va_surface();
            drv_ctx->vtable->vaDestroySurfaces(drv_ctx, &surface, 1);
            delete buf;
        };

        return VAAPIBufferPtr(new VAAPIBuffer(va_surface_id, info, _vaapi_context), deleter);
    }

  private:
    VAAPIContextPtr _vaapi_context;
};

#else // !ENABLE_VAAPI

class BufferMapperDMAToVAAPI final : public BufferMapper {
  public:
    BufferMapperDMAToVAAPI(ContextPtr) {
        // STUB
        throw std::runtime_error("Couldn't create DMA to VAAPI mapper: project was built without VAAPI support");
    }

    BufferPtr map(BufferPtr, AccessMode) override {
        return nullptr;
    }

    VAAPIBufferPtr map(DMABufferPtr, AccessMode) {
        return nullptr;
    }
};

#endif

} // namespace dlstreamer
