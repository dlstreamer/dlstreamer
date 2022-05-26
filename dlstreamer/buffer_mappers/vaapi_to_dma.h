/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/dma/buffer.h"
#include "dlstreamer/vaapi/buffer.h"
#include "dlstreamer/vaapi/context.h"

#ifdef ENABLE_VAAPI
#include <va/va_backend.h>
#include <va/va_drmcommon.h>
#endif

namespace dlstreamer {

#ifdef ENABLE_VAAPI

class BufferMapperVAAPIToDMA final : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto va_buffer = std::dynamic_pointer_cast<VAAPIBuffer>(src_buffer);
        if (!va_buffer)
            throw std::runtime_error("Invalid buffer type: VAAPI buffer is expected");
        return map(va_buffer, mode);
    }

    DMABufferPtr map(VAAPIBufferPtr buffer, AccessMode mode) {
        auto context = std::dynamic_pointer_cast<VAAPIContext>(buffer->context());
        if (!context)
            throw std::runtime_error("Invalid buffer context type: VAAPI context is expected");
        return map_internal(buffer, context, mode);
    }

  private:
    DMABufferPtr map_internal(VAAPIBufferPtr buffer, VAAPIContextPtr context, AccessMode /*mode*/) {
        auto driver_context = reinterpret_cast<VADisplayContextP>(context->va_display())->pDriverContext;
        if (!driver_context)
            throw std::runtime_error("VA driver context is null");
        auto vtable = driver_context->vtable;

        VADRMPRIMESurfaceDescriptor prime_desc{};
        vtable->vaExportSurfaceHandle(driver_context, buffer->va_surface(), VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_WRITE, &prime_desc);

        const int dma_fd = prime_desc.objects[0].fd;
        const int drm_format_modifier = prime_desc.objects[0].drm_format_modifier; // non-zero if tiled (non-linear) mem

        // FIXME: Probably DMA size should be placed to DMABuffer (similar to drm format modifier)?
#if 0
        *out_dma_size = prime_desc.objects[0].size;
#endif

        // Update stride and offset for each plane
        auto info = std::make_shared<BufferInfo>(*buffer->info());

        uint32_t plane_num = 0;
        for (uint32_t i = 0; i < prime_desc.num_layers; i++) {
            const auto layer = &prime_desc.layers[i];
            for (uint32_t j = 0; j < layer->num_planes; j++) {
                if (plane_num >= info->planes.size())
                    break;
                auto &plane = info->planes.at(plane_num);

                plane.stride[plane.layout.w_position() - 1] = layer->pitch[j];
                plane.offset = layer->offset[j];
                plane_num++;
            }
        }

        auto deleter = [buffer, dma_fd](DMABuffer *buf) {
            close(dma_fd);
            delete buf;
        };
        return std::shared_ptr<DMABuffer>(new DMABuffer(dma_fd, drm_format_modifier, std::move(info)), deleter);
    }
};

#else

class BufferMapperVAAPIToDMA final : public BufferMapper {
  public:
    BufferMapperVAAPIToDMA() {
        // STUB
        throw std::runtime_error("Couldn't create VAAPI to DMA mapper: project was built without VAAPI support");
    }

    BufferPtr map(BufferPtr, AccessMode) override {
        return nullptr;
    }
};

#endif // ENABLE_VAAPI

} // namespace dlstreamer
