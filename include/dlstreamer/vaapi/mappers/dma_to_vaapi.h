/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/dma/context.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/utils.h"
#include "dlstreamer/vaapi/frame.h"
#include "dlstreamer/vaapi/utils.h"

#include <string>
#include <va/va.h>
#include <va/va_backend.h>
#include <va/va_drmcommon.h>

namespace dlstreamer {

class MemoryMapperDMAToVAAPI : public BaseMemoryMapper {
  public:
    MemoryMapperDMAToVAAPI(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
        DLS_CHECK(output_context)
        auto va_display = output_context->handle(BaseContext::key::va_display);
        DLS_CHECK(va_display)
        DLS_CHECK(_drv_ctx = reinterpret_cast<VADisplayContextP>(va_display)->pDriverContext);
    }

    TensorPtr map(TensorPtr /*tensor*/, AccessMode /*mode*/) override {
        throw std::invalid_argument("Tensor mapping not supported");
    }

    FramePtr map(FramePtr src, AccessMode /*mode*/) override {
        auto info = frame_info(src);
        auto tensor0 = src->tensor(0);
        for (size_t i = 1; i < src->num_tensors(); i++) {
            if (src->tensor(i)->handle(tensor::key::dma_fd) != tensor0->handle(tensor::key::dma_fd))
                throw std::invalid_argument("Expect all tensors on same DMA buffer");
        }

        VASurfaceID va_surface_id = dma_to_va_surface(tensor0, info);
        auto drv_ctx = _drv_ctx;
        auto deleter = [drv_ctx](VAAPIFrame *buf) {
            auto surface = buf->va_surface();
            drv_ctx->vtable->vaDestroySurfaces(drv_ctx, &surface, 1);
            delete buf;
        };
        auto ret = VAAPIFramePtr(new VAAPIFrame(va_surface_id, info, _output_context), deleter);

        ret->set_parent(src);
        return ret;
    }

  private:
    VASurfaceID dma_to_va_surface(TensorPtr tensor, const FrameInfo &info) {
        uint32_t video_format = video_format_to_vaapi(static_cast<ImageFormat>(info.format));
        uint32_t rtformat = vaapi_video_format_to_rtformat(video_format);

        // width and height from first plane
        VASurfaceAttribExternalBuffers external{};
        ImageInfo image_info(info.tensors.front());
        external.width = image_info.width();
        external.height = image_info.height();
        if (image_info.layout().n_position() >= 0) {
            external.height *= image_info.batch();
        }
        external.num_planes = info.tensors.size();

        auto dma_tensor = ptr_cast<DMATensor>(tensor);
        uint64_t dma_fd = dma_tensor->dma_fd();
        external.buffers = &dma_fd;
        external.num_buffers = 1;
        external.pixel_format = video_format;
        external.data_size = 0;
        for (size_t i = 0; i < external.num_planes; i++) {
            external.pitches[i] = ImageInfo(info.tensors[i]).width_stride();
            external.offsets[i] = dma_tensor->offset();
            external.data_size += info.tensors[i].nbytes();
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
        VAStatus status = _drv_ctx->vtable->vaCreateSurfaces2(_drv_ctx, rtformat, external.width, external.height,
                                                              &va_surface_id, 1, attribs, 2);
        if (status != VA_STATUS_SUCCESS)
            throw std::runtime_error("Coulnd't create VASurface from DMA: vaCreateSurfaces2 failed, " +
                                     std::to_string(status));
        return va_surface_id;
    }

  protected:
    VADriverContextP _drv_ctx;
};

} // namespace dlstreamer
