/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/ffmpeg/utils.h"
#include "dlstreamer/frame_info.h"
#include "dlstreamer/vaapi/tensor.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}

namespace dlstreamer {

class FFmpegFrame : public BaseFrame {
  public:
    FFmpegFrame() : BaseFrame(MediaType::Image, 0, MemoryType::FFmpeg) {
        _frames.push_back(av_frame_alloc());
    }

    FFmpegFrame(AVFrame *frame, bool take_ownership = false, ContextPtr context = nullptr)
        : BaseFrame(MediaType::Image, 0, MemoryType::FFmpeg), _frames({frame}), _take_ownership(take_ownership) {
        init(_frames, avframe_to_info(frame), context);
    }

    FFmpegFrame(const std::vector<AVFrame *> &batched_frames, bool take_ownership = false, ContextPtr context = nullptr)
        : BaseFrame(MediaType::Image, 0, MemoryType::FFmpeg), _frames({batched_frames}),
          _take_ownership(take_ownership) {
        init(_frames, avframe_to_info(batched_frames.front()), context);
    }

    FFmpegFrame(AVFrame *frame, bool take_ownership, const FrameInfo &info, ContextPtr context = nullptr)
        : BaseFrame(MediaType::Image, 0, MemoryType::FFmpeg), _frames({frame}), _take_ownership(take_ownership) {
        init(_frames, info, context);
    }

    ~FFmpegFrame() {
        if (_take_ownership) {
            for (auto av_frame : _frames)
                av_frame_free(&av_frame);
        }
    }

    void init(const std::vector<AVFrame *> &frames, const FrameInfo &info, ContextPtr context) {
        for (AVFrame *frame : frames) {
            if (frame->format == AV_PIX_FMT_VAAPI) {
                auto va_surface = (uint32_t)(size_t)frame->data[3]; // As defined by AV_PIX_FMT_VAAPI
                _tensors.push_back(std::make_shared<VAAPITensor>(va_surface, 0, info.tensors[0], context));
            } else {
                for (size_t i = 0; i < AV_NUM_DATA_POINTERS; i++) {
                    if (!frame->data[i])
                        break;
                    _tensors.push_back(std::make_shared<CPUTensor>(info.tensors[i], frame->data[i]));
                }
            }
        }
    }

    AVFrame *avframe() {
        return _frames.front();
    }

  private:
    std::vector<AVFrame *> _frames;
    bool _take_ownership = false;
};

using FFmpegFramePtr = std::shared_ptr<FFmpegFrame>;

} // namespace dlstreamer
