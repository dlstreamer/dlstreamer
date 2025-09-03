/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/blocking_queue.h"
#include "dlstreamer/base/source.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/frame.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/source.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/elements/vaapi_batch_proc.h"
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

class MultiSourceFFMPEG : public BaseSource {
  public:
    MultiSourceFFMPEG(DictionaryCPtr params, const ContextPtr &app_context) : BaseSource(app_context) {
        _ffmpeg_ctx = ptr_cast<FFmpegContext>(app_context);

        auto inputs = params->get<std::vector<std::string>>("inputs");
        for (auto &input : inputs)
            add_input(input);
    }

    ~MultiSourceFFMPEG() {
        for (auto &stream : _streams) {
            stream.active = false;
            stream.thread.join();
        }
    }

    void add_input(std::string_view url) {
        // avformat_open_input
        AVInputFormat *input_format = NULL; // av_find_input_format(format.c_str());
        AVFormatContext *input_ctx = NULL;
        DLS_CHECK_GE0(avformat_open_input(&input_ctx, url.data(), input_format, NULL));

        // av_find_best_stream
        const AVCodec *codec = nullptr;
        int video_stream = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        DLS_CHECK_GE0(video_stream);
        AVCodecParameters *codecpar = input_ctx->streams[video_stream]->codecpar;
        int64_t time_delta = static_cast<int64_t>(1e9 / av_q2d(input_ctx->streams[video_stream]->avg_frame_rate));

        // avcodec_open2
        AVCodecContext *decoder_ctx = NULL;
        DLS_CHECK(decoder_ctx = avcodec_alloc_context3(codec));
        DLS_CHECK_GE0(avcodec_parameters_to_context(decoder_ctx, codecpar));
        decoder_ctx->hw_device_ctx = av_buffer_ref(_ffmpeg_ctx->hw_device_context_ref());
        decoder_ctx->get_format = [](AVCodecContext * /*ctx*/, const enum AVPixelFormat * /*pix_fmts*/) {
            return AV_PIX_FMT_VAAPI; // request VAAPI frame format
        };
        DLS_CHECK_GE0(avcodec_open2(decoder_ctx, codec, NULL));

        // TODO fill _output_info

        // Create thread with frame reading loop
        size_t stream_id = _streams.size();
        _streams.push_back({});
        _streams.back().thread = std::thread([=, this] {
            int64_t timestamp = 0;
            _streams[stream_id].active = true;
            while (_streams[stream_id].active) {
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

                    FramePtr frame = std::make_shared<FFmpegFrame>(dec_frame, true, _ffmpeg_ctx);

                    if (_postproc)
                        frame = _postproc->process(frame);

                    timestamp += time_delta;
                    SourceIdentifierMetadata meta(frame->metadata().add(SourceIdentifierMetadata::name));
                    auto pts = (dec_frame->pts == AV_NOPTS_VALUE) ? timestamp : dec_frame->pts;
                    meta.init(0, pts, stream_id, 0);

                    _queue.push(frame, MAX_QUEUE_SIZE);
                }

                if (avpacket)
                    av_packet_free(&avpacket);
                else
                    break; // End of stream on av_read_frame() call
            }

            FramePtr null_frame(nullptr);
            _queue.push(null_frame);
            _streams[stream_id].active = false;

            AVFormatContext *in_ctx = input_ctx;
            avformat_close_input(&in_ctx);
        });
    }

    ContextPtr get_context(MemoryType memory_type) noexcept override {
        if (memory_type == MemoryType::FFmpeg)
            return _ffmpeg_ctx;
        else if (memory_type == MemoryType::VAAPI)
            return _vaapi_ctx;
        return nullptr;
    }

    void set_output_info(const FrameInfo &info) override {
        TransformPtr vaapi_preproc;
        // if (hw_device_type() == AV_HWDEVICE_TYPE_VAAPI)
        _vaapi_ctx = VAAPIContext::create(_ffmpeg_ctx);
        _postproc = create_transform(vaapi_batch_proc, {}, _vaapi_ctx);
        _postproc->set_output_info(info);
        _output_info = info;
    }

    FramePtr read() override {
        return _queue.pop();
    }

  private:
    FFmpegContextPtr _ffmpeg_ctx;
    VAAPIContextPtr _vaapi_ctx;
    TransformPtr _postproc;
    BlockingQueue<FramePtr> _queue;

    struct StreamState {
        bool active;
        std::thread thread;
    };
    std::vector<StreamState> _streams;
};

extern "C" {
DLS_EXPORT ElementDesc ffmpeg_multi_source = {.name = "ffmpeg_multi_source",
                                              .description = "Multi video-stream source element based on FFmpeg",
                                              .author = "Intel Corporation",
                                              .params = nullptr,
                                              .input_info = MAKE_FRAME_INFO_VECTOR({}),
                                              .output_info = MAKE_FRAME_INFO_VECTOR({{MediaType::Image}}),
                                              .create = create_element<MultiSourceFFMPEG>,
                                              .flags = 0};
}

} // namespace dlstreamer
