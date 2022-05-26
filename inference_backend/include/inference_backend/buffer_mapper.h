/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_mappers/gst_to_cpu.h"
#include "dlstreamer/buffer_mappers/gst_to_dma.h"
#include "dlstreamer/buffer_mappers/gst_to_vaapi.h"
#include "inference_backend/image.h"

namespace InferenceBackend {

// This class is used as shim between dlstreamer::Buffer and InferenceBackend::Image
// The whole mapping process is handled by dlstreamer::BufferMapper
class BufferToImageMapper final {
  public:
    BufferToImageMapper(InferenceBackend::MemoryType memory_type, const GstVideoInfo *video_info,
                        dlstreamer::BufferMapperPtr mapper)
        : _memory_type(memory_type), _video_info(video_info), _mapper(mapper) {
    }
    ~BufferToImageMapper() = default;

    InferenceBackend::MemoryType memoryType() const {
        return _memory_type;
    }

    InferenceBackend::ImagePtr map(GstBuffer *gst_buffer, GstMapFlags flags) {
        auto gbuffer = std::make_shared<dlstreamer::GSTBuffer>(gst_buffer, _video_info);
        int mode = 0;
        if (flags & GST_MAP_READ)
            mode |= static_cast<int>(dlstreamer::AccessMode::READ);
        if (flags & GST_MAP_WRITE)
            mode |= static_cast<int>(dlstreamer::AccessMode::WRITE);

        dlstreamer::BufferPtr buffer = _mapper->map(gbuffer, static_cast<dlstreamer::AccessMode>(mode));

        ImagePtr image;
        if (_memory_type == MemoryType::SYSTEM) {
            // For system memory, the buffer must remain mapped the lifetime of the image
            auto deleter = [buffer](Image *img) mutable {
                buffer.reset();
                delete img;
            };
            image = {new Image(), deleter};
        } else {
            // For VAAPI and DMA - we don't care because we only need handles.
            image = std::make_shared<Image>();
        }

        dlstreamer::BufferInfoCPtr info = buffer->info();
        image->format = info->format;
        size_t size = 0;
        for (size_t i = 0; i < info->planes.size(); i++) {
            image->planes[i] = static_cast<uint8_t *>(buffer->data(i));
            image->offsets[i] = info->planes[i].offset;
            image->stride[i] = info->planes[i].width_stride();
            size += info->planes[i].size();
        }
        image->width = info->planes[0].width();
        image->height = info->planes[0].height();
        image->size = size;
        image->type = _memory_type;
        if (_memory_type == MemoryType::VAAPI) {
            auto vaapi_buffer = std::dynamic_pointer_cast<dlstreamer::VAAPIBuffer>(buffer);
            auto vaapi_context = std::dynamic_pointer_cast<dlstreamer::VAAPIContext>(buffer->context());
            if (!vaapi_buffer || !vaapi_context)
                throw std::runtime_error("VAAPI types casting failed");
            image->va_surface_id = vaapi_buffer->va_surface();
            image->va_display = vaapi_context->va_display();
        };
        image->dma_fd = buffer->handle(dlstreamer::DMABuffer::dma_fd_id, 0, 0);
        image->drm_format_modifier = buffer->handle(dlstreamer::DMABuffer::drm_modifier_id, 0, 0);

        return image;
    }

  private:
    InferenceBackend::MemoryType _memory_type;
    const GstVideoInfo *_video_info;
    dlstreamer::BufferMapperPtr _mapper;
};

using BufferMapper = BufferToImageMapper;

} // namespace InferenceBackend

class BufferMapperFactory {
  public:
    static dlstreamer::BufferMapperPtr createMapper(InferenceBackend::MemoryType memory_type,
                                                    dlstreamer::ContextPtr dst_context = nullptr) {
        switch (memory_type) {
        case InferenceBackend::MemoryType::SYSTEM:
            return std::make_shared<dlstreamer::BufferMapperGSTToCPU>();
        case InferenceBackend::MemoryType::DMA_BUFFER:
            return std::make_shared<dlstreamer::BufferMapperGSTToDMA>();
        case InferenceBackend::MemoryType::VAAPI:
            return std::make_shared<dlstreamer::BufferMapperGSTToVAAPI>(dst_context);
        case InferenceBackend::MemoryType::USM_DEVICE_POINTER:
            throw std::runtime_error("Not impemented");
        case InferenceBackend::MemoryType::ANY:
        default:
            break;
        }
        throw std::runtime_error("MemoryType not specified");
    }

    static std::unique_ptr<InferenceBackend::BufferToImageMapper>
    createMapper(InferenceBackend::MemoryType dst_memory_type, const GstVideoInfo *intput_video_info,
                 dlstreamer::ContextPtr dst_context = nullptr) {
        dlstreamer::BufferMapperPtr mapper = createMapper(dst_memory_type, dst_context);
        return std::unique_ptr<InferenceBackend::BufferToImageMapper>(
            new InferenceBackend::BufferToImageMapper(dst_memory_type, intput_video_info, mapper));
    }

    BufferMapperFactory() = delete;
};
