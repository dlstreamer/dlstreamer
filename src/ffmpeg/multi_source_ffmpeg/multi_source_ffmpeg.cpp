/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "blocking_queue.h"
#include "dlstreamer/base/element.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/frame.h"
#include "dlstreamer/source.h"
#include "dlstreamer/vaapi/context.h"
#include <thread>

extern "C" {
// FFmpeg
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

#define MAX_QUEUE_SIZE 16

namespace dlstreamer {

class MultiSourceFFMPEG : public BaseElement<Source> {
  public:
    MultiSourceFFMPEG(DictionaryCPtr params, const ContextPtr &app_context) : _app_context(app_context) {
        auto device = params->get("device", std::string());
        char *device_str = device.empty() ? nullptr : device.data();
        AVBufferRef *hw_device_ctx;
        DLS_CHECK_GE0(av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, device_str, NULL, 0));
        _ffmpeg_ctx = FFmpegContext::create(hw_device_ctx);

        auto input = params->get<std::string>("input");
        add_input(input);
    }

    ~MultiSourceFFMPEG() {
        for (auto &stream : _streams) {
            stream.second.active = false;
            stream.second.thread.join();
        }
    }

    void add_input(std::string_view url) {
        // avformat_open_input
        AVInputFormat *input_format = NULL; // av_find_input_format(format.c_str());
        AVFormatContext *input_ctx = NULL;
        DLS_CHECK_GE0(avformat_open_input(&input_ctx, url.data(), input_format, NULL));

        // av_find_best_stream
        AVCodec *codec = nullptr;
        int video_stream = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        DLS_CHECK_GE0(video_stream);
        AVCodecParameters *codecpar = input_ctx->streams[video_stream]->codecpar;

        // avcodec_open2
        AVCodecContext *decoder_ctx = NULL;
        DLS_CHECK(decoder_ctx = avcodec_alloc_context3(codec));
        DLS_CHECK_GE0(avcodec_parameters_to_context(decoder_ctx, codecpar));
        decoder_ctx->hw_device_ctx = av_buffer_ref(_ffmpeg_ctx->hw_device_context_ref());
        decoder_ctx->get_format = [](AVCodecContext * /*ctx*/, const enum AVPixelFormat * /*pix_fmts*/) {
            return AV_PIX_FMT_VAAPI; // request VAAPI frame format
        };
        DLS_CHECK_GE0(avcodec_open2(decoder_ctx, codec, NULL));

        StreamState *stream = &_streams[std::string(url)];
        stream->thread = std::thread([=] {
            stream->active = true;
            while (stream->active) {
                // Read packet with compressed video frame
                AVPacket *avpacket = av_packet_alloc();
                if (av_read_frame(input_ctx, avpacket) < 0) {
                    av_packet_free(&avpacket); // EOF or error. Send NULL to avcodec_send_packet once to flush decoder
                } else if (avpacket->stream_index != video_stream) {
                    av_packet_free(&avpacket); // Non-video (ex, audio) packet
                    continue;
                }

                // Send packet to decoder
                DLS_CHECK_GE0(avcodec_send_packet(decoder_ctx, avpacket));

                for (;;) {
                    // Receive frame from decoder
                    auto dec_frame = av_frame_alloc();
                    int decode_err = avcodec_receive_frame(decoder_ctx, dec_frame);
                    if (decode_err == AVERROR(EAGAIN) || decode_err == AVERROR_EOF) {
                        break;
                    }
                    DLS_CHECK_GE0(decode_err);
                    // static int frame_num = 0;
                    // printf("--------------------------------------- Frame %d\n", frame_num++);

                    auto frame = std::make_shared<FFmpegFrame>(dec_frame, true, _ffmpeg_ctx);

                    _queue.push(frame, MAX_QUEUE_SIZE);
                }

                if (avpacket)
                    av_packet_free(&avpacket);
                else
                    break; // End of stream on av_read_frame() call
            }

            _queue.push(nullptr);
            stream->active = false;

            AVFormatContext *in_ctx = input_ctx;
            avformat_close_input(&in_ctx);
        });
    }

    ContextPtr get_context(MemoryType memory_type) noexcept override {
        if (memory_type == MemoryType::FFmpeg)
            return _ffmpeg_ctx;
        return nullptr;
    }

    FrameInfoVector get_output_info() override {
        throw std::runtime_error("TODO");
    }

    FramePtr read() override {
        return _queue.pop();
    }

  private:
    ContextPtr _app_context;
    FFmpegContextPtr _ffmpeg_ctx;
    blocking_queue<FramePtr> _queue;

    struct StreamState {
        bool active;
        std::thread thread;
    };
    std::map<std::string, StreamState> _streams;
};

extern "C" {
DLS_EXPORT ElementDesc multi_source_ffmpeg = {.name = "multi_source_ffmpeg",
                                              .description = "Multi video-stream source element based on FFmpeg",
                                              .author = "Intel Corporation",
                                              .params = nullptr,
                                              .input_info = {},
                                              .output_info = {{MediaType::Image}},
                                              .create = create_element<MultiSourceFFMPEG>,
                                              .flags = 0};
}

} // namespace dlstreamer
