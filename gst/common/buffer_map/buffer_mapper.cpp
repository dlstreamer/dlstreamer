/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "buffer_mapper.h"

#include <string>

#include <gst/allocators/allocators.h>
#include <safe_arithmetic.hpp>

#include "gva_utils.h"

VideoBufferMapper::VideoBufferMapper(const GstVideoInfo &info) : vinfo_(gst_video_info_copy(&info)) {
    if (!vinfo_)
        throw std::runtime_error("Couldn't copy video info");

    const guint n_planes = GST_VIDEO_INFO_N_PLANES(vinfo_);
    if (n_planes == 0 or n_planes > InferenceBackend::Image::MAX_PLANES_NUMBER)
        throw std::logic_error("Image planes number " + std::to_string(n_planes) + " isn't supported");

    // Prepare boilerplate
    fillImageFromVideoInfo_(vinfo_, image_boilerplate_);
}

VideoBufferMapper::~VideoBufferMapper() {
    gst_video_info_free(vinfo_);
    vinfo_ = nullptr;
}

void VideoBufferMapper::fillImageFromVideoInfo_(const GstVideoInfo *vinfo, InferenceBackend::Image &image) {
    image.format = gst_format_to_fourcc(GST_VIDEO_INFO_FORMAT(vinfo));
    image.width = safe_convert<uint32_t>(GST_VIDEO_INFO_WIDTH(vinfo));
    image.height = safe_convert<uint32_t>(GST_VIDEO_INFO_HEIGHT(vinfo));
    image.size = safe_convert<uint32_t>(GST_VIDEO_INFO_SIZE(vinfo));
    for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES(vinfo); ++i) {
        image.stride[i] = safe_convert<uint32_t>(GST_VIDEO_INFO_PLANE_STRIDE(vinfo, i));
        image.offsets[i] = safe_convert<uint32_t>(GST_VIDEO_INFO_PLANE_OFFSET(vinfo, i));
    }
}

class SystemBufferMapper : public VideoBufferMapper {
    struct MapContext {
        constexpr static uint32_t MAGIC_VALUE = 0xC00FFEE;

        uint32_t magic = MAGIC_VALUE;
        SystemBufferMapper *parent = nullptr;
        GstVideoFrame frame;

        ~MapContext() {
            magic = 0xDEADDEAD;
            parent = nullptr;
        }

        bool valid() const {
            return magic == MAGIC_VALUE;
        }
    };

  public:
    SystemBufferMapper(const GstVideoInfo &info) : VideoBufferMapper(info) {
        image_boilerplate_.type = memoryType();
    }

    InferenceBackend::MemoryType memoryType() const override {
        return InferenceBackend::MemoryType::SYSTEM;
    }

    InferenceBackend::Image map(GstBuffer *buffer, GstMapFlags flags) override {
        InferenceBackend::Image image = image_boilerplate_;

        auto map_context = std::unique_ptr<MapContext>(new MapContext());

        if (!gst_video_frame_map(&map_context->frame, vinfo_, buffer, flags)) {
            throw std::runtime_error("Failed to map GstBuffer to system memory");
        }

        for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES(vinfo_); ++i) {
            image.planes[i] = static_cast<uint8_t *>(GST_VIDEO_FRAME_PLANE_DATA(&map_context->frame, i));
        }

        for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES(vinfo_); ++i) {
            image.stride[i] = safe_convert<uint32_t>(GST_VIDEO_FRAME_PLANE_STRIDE(&map_context->frame, i));
            image.offsets[i] = safe_convert<uint32_t>(GST_VIDEO_FRAME_PLANE_OFFSET(&map_context->frame, i));
        }

        image.map_context = map_context.release();

        // TODO: move to separate class?
#ifdef ENABLE_VPUX
        auto mem = GstMemoryUniquePtr(gst_buffer_get_memory(buffer, 0), gst_memory_unref);
        if (not mem.get())
            throw std::runtime_error("Failed to get GstBuffer memory");
        if (gst_is_dmabuf_memory(mem.get())) {
            int dma_fd = gst_dmabuf_memory_get_fd(mem.get());
            if (dma_fd <= 0)
                throw std::runtime_error("Failed to get file desc associated with GstBuffer memory");
            image.dma_fd = dma_fd;
        }
#endif

        return image;
    }

    void unmap(InferenceBackend::Image &image) override {
        if (!image.map_context)
            return;

        auto ctx = static_cast<MapContext *>(image.map_context);
        if (!ctx->valid())
            throw std::runtime_error("Couldn't unmap image: invalid map contex");

        image.map_context = nullptr;
        gst_video_frame_unmap(&ctx->frame);
        delete ctx;
    }
};

class DmaBufferMapper : public VideoBufferMapper {
  public:
    DmaBufferMapper(const GstVideoInfo &info) : VideoBufferMapper(info) {
        image_boilerplate_.type = memoryType();
    }

    InferenceBackend::MemoryType memoryType() const override {
        return InferenceBackend::MemoryType::DMA_BUFFER;
    }

    InferenceBackend::Image map(GstBuffer *buffer, GstMapFlags) override {
        InferenceBackend::Image image = image_boilerplate_;

        GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
        if (!mem)
            throw std::runtime_error("Failed to get GstBuffer memory");

        image.dma_fd = gst_dmabuf_memory_get_fd(mem);
        if (image.dma_fd < 0)
            throw std::runtime_error("Failed to import DMA buffer FD");

        return image;
    }

    void unmap(InferenceBackend::Image &) override {
    }
};

class VaapiBufferMapper : public VideoBufferMapper {
    constexpr static unsigned int INVALID_SURFACE_ID = 0xffffffff;
    constexpr static GstMapFlags GST_MAP_VA = static_cast<GstMapFlags>(GST_MAP_FLAG_LAST << 1);

  public:
    VaapiBufferMapper(const GstVideoInfo &info, VaApiDisplayPtr display) : VideoBufferMapper(info), display_(display) {
        image_boilerplate_.type = memoryType();
    }

    InferenceBackend::MemoryType memoryType() const override {
        return InferenceBackend::MemoryType::VAAPI;
    }

    InferenceBackend::Image map(GstBuffer *buffer, GstMapFlags) override {
        GstMapInfo map_info;

        GstMapFlags flags = GST_MAP_VA;
        if (!gst_buffer_map(buffer, &map_info, flags)) {
            flags = static_cast<GstMapFlags>(flags | GST_MAP_READ);
            if (!gst_buffer_map(buffer, &map_info, flags)) {
                throw std::runtime_error("Couldn't map buffer (VAAPI memory)");
            }
        }

        auto surface = *reinterpret_cast<const unsigned int *>(map_info.data);

        gst_buffer_unmap(buffer, &map_info);

        if (surface == INVALID_SURFACE_ID)
            throw std::runtime_error("Got invalid surface after map (VAAPI memory)");

        InferenceBackend::Image image = image_boilerplate_;

        image.va_surface_id = surface;
        image.va_display = display_.get();

        return image;
    }

    void unmap(InferenceBackend::Image &) override {
    }

  protected:
    VaApiDisplayPtr display_;
};

std::unique_ptr<BufferMapper> BufferMapperFactory::createMapper(InferenceBackend::MemoryType memory_type,
                                                                const GstVideoInfo *info) {
    return createMapper(memory_type, info, {});
}

std::unique_ptr<BufferMapper> BufferMapperFactory::createMapper(InferenceBackend::MemoryType memory_type,
                                                                const GstVideoInfo *info, VaApiDisplayPtr va_dpy) {
    using namespace InferenceBackend;

    if (!info)
        throw std::invalid_argument("info: pointer is null");

    switch (memory_type) {
    case MemoryType::SYSTEM:
        return std::unique_ptr<BufferMapper>(new SystemBufferMapper(*info));

    case MemoryType::DMA_BUFFER:
        return std::unique_ptr<BufferMapper>(new DmaBufferMapper(*info));

    case MemoryType::VAAPI:
        if (!va_dpy)
            throw std::invalid_argument("va_dpy: for VAAPI memory type the VADisplay must be provided");
        return std::unique_ptr<BufferMapper>(new VaapiBufferMapper(*info, va_dpy));

    default:
        break;
    }

    throw std::invalid_argument("memory_type: unsupported type");
}
