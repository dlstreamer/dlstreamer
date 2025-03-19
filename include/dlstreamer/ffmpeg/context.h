/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"
#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/utils.h"
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

namespace dlstreamer {

class FFmpegContext;
using FFmpegContextPtr = std::shared_ptr<FFmpegContext>;

class FFmpegContext : public BaseContext {
  public:
    struct key {
        static constexpr auto device_context = "ffmpeg.device_context"; // AVBufferRef*
        static constexpr auto va_display = BaseContext::key::va_display;
    };

    FFmpegContext(AVHWDeviceType hw_device_type = AV_HWDEVICE_TYPE_VAAPI, const char *device = nullptr)
        : BaseContext(MemoryType::FFmpeg), _hw_device_ctx(0), _take_ownerwhip(true) {
        DLS_CHECK_GE0(av_hwdevice_ctx_create(&_hw_device_ctx, hw_device_type, device, NULL, 0));
    }

    FFmpegContext(AVBufferRef *hw_device_ctx, bool take_ownerwhip = true)
        : BaseContext(MemoryType::FFmpeg), _hw_device_ctx(hw_device_ctx), _take_ownerwhip(take_ownerwhip) {
        if (take_ownerwhip)
            DLS_CHECK(hw_device_ctx);
    }

    FFmpegContext(std::string_view accel_type = std::string_view(), std::string_view device = std::string_view())
        : BaseContext(MemoryType::FFmpeg) {
        if (!accel_type.empty()) {
            auto hwdevice_type = av_hwdevice_find_type_by_name(accel_type.data());
            DLS_CHECK(hwdevice_type != AV_HWDEVICE_TYPE_NONE);
            const char *device_ptr = !device.empty() ? device.data() : NULL;
            DLS_CHECK(av_hwdevice_ctx_create(&_hw_device_ctx, hwdevice_type, device_ptr, NULL, 0) >= 0);
        }
    }

    virtual ~FFmpegContext() {
        if (_take_ownerwhip)
            av_buffer_unref(&_hw_device_ctx);
    }

    AVBufferRef *hw_device_context_ref() const {
        return _hw_device_ctx;
    }

    AVHWDeviceContext *hw_device_context() const {
        if (!_hw_device_ctx)
            return NULL;
        return reinterpret_cast<AVHWDeviceContext *>(_hw_device_ctx->data);
    }

    AVHWDeviceType hw_device_type() const {
        if (!hw_device_context())
            return AV_HWDEVICE_TYPE_NONE;
        return hw_device_context()->type;
    }

    std::vector<std::string> keys() const override {
        return {key::device_context};
    }

    void *handle(std::string_view key) const noexcept override {
        if (key == key::device_context || key.empty())
            return _hw_device_ctx;
        if (key == key::va_display && _hw_device_ctx) {
            AVHWDeviceContext *device_ctx = (AVHWDeviceContext *)_hw_device_ctx->data;
            if (device_ctx->type == AV_HWDEVICE_TYPE_VAAPI)
                return *(void **)(device_ctx->hwctx);
        }
        return nullptr;
    }

    MemoryMapperPtr get_mapper(const ContextPtr &input_context, const ContextPtr &output_context) override {
        auto mapper = BaseContext::get_mapper(input_context, output_context);
        if (mapper)
            return mapper;

        auto input_type = input_context ? input_context->memory_type() : MemoryType::CPU;
        auto output_type = output_context ? output_context->memory_type() : MemoryType::CPU;
        if (input_type == MemoryType::FFmpeg && output_type == MemoryType::CPU) {
            if (!_hw_device_ctx)
                return std::make_shared<BaseMemoryMapper>(input_context, output_context);
        }
        if (input_type == MemoryType::FFmpeg && output_type == MemoryType::VAAPI) {
            if (_hw_device_ctx) {
                AVHWDeviceContext *device_ctx = (AVHWDeviceContext *)_hw_device_ctx->data;
                if (device_ctx->type == AV_HWDEVICE_TYPE_VAAPI)
                    return std::make_shared<BaseMemoryMapper>(input_context, output_context);
            }
        }

        BaseContext::attach_mapper(mapper);
        return mapper;
    }

  private:
    AVBufferRef *_hw_device_ctx = nullptr;
    bool _take_ownerwhip = false;
};

} // namespace dlstreamer

#if 0
//////////////////////////////////////////////////////////////////////////
// Supported memory mappers

#include "dlstreamer/vaapi/mappers/dma_to_vaapi.h"
#include "dlstreamer/vaapi/mappers/vaapi_to_dma.h"

namespace dlstreamer {

inline MemoryMapperPtr VAAPIContext::get_mapper(const ContextPtr& input_context, const ContextPtr& output_context) {
    auto mapper = BaseContext::get_mapper(input_context, output_context);
    if (mapper)
        return mapper;

    auto input_type = input_context->memory_type();
    auto output_type = output_context->memory_type();
    if (input_type == MemoryType::VAAPI && output_type == MemoryType::DMA)
        mapper = std::make_shared<MemoryMapperVAAPIToDMA>();
    if (input_type == MemoryType::DMA && output_type == MemoryType::VAAPI)
        mapper = std::make_shared<MemoryMapperDMAToVAAPI>(output_context);

    if (mapper)
        BaseContext::attach
}

} // namespace dlstreamer
#endif
