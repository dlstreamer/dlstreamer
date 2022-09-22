/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <CL/sycl.hpp>

#include "dlstreamer/dma/context.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/frame.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/transform.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/elements/video_preproc_vaapi.h"

#include <gflags/gflags.h>
#include <memory>

DEFINE_string(i, "", "Required. Path to input video file");
DEFINE_string(device, "GPU", "Device for decode and inference, 'CPU' or 'GPU'");
DEFINE_uint64(width, 640, "Width of output grayscale images");
DEFINE_uint64(height, 480, "Height of output grayscale images");

using namespace dlstreamer;

int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    DLS_CHECK(!FLAGS_i.empty());

    // DPC++ queue
    sycl::queue sycl_queue = (FLAGS_device.find("GPU") != std::string::npos) ? sycl::queue(sycl::gpu_selector())
                                                                             : sycl::queue(sycl::host_selector());
    // find video stream information
    AVInputFormat *input_format = NULL; // av_find_input_format(format.c_str());
    AVFormatContext *input_ctx = NULL;
    DLS_CHECK_GE0(avformat_open_input(&input_ctx, FLAGS_i.data(), input_format, NULL));
    AVCodec *codec = nullptr;
    int video_stream = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    DLS_CHECK_GE0(video_stream);
    AVCodecParameters *codecpar = input_ctx->streams[video_stream]->codecpar;

    // create CPU-based or VAAPI-based decoder depending on 'device' parameter
    AVCodecContext *decoder_ctx = NULL;
    DLS_CHECK(decoder_ctx = avcodec_alloc_context3(codec));
    DLS_CHECK_GE0(avcodec_parameters_to_context(decoder_ctx, codecpar));
    if (FLAGS_device.find("GPU") != std::string::npos) {
        const char *device = nullptr;
        DLS_CHECK_GE0(av_hwdevice_ctx_create(&decoder_ctx->hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, device, NULL, 0));
        decoder_ctx->get_format = [](AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
            return AV_PIX_FMT_VAAPI; // request VAAPI frame format
        };
    }
    DLS_CHECK_GE0(avcodec_open2(decoder_ctx, codec, NULL));

    // Create Intel® Deep Learning Streamer (Intel® DL Streamer) context objects for FFmpeg, VAAPI and DPCPP
    auto ffmpeg_ctx = FFmpegContext::create(decoder_ctx->hw_device_ctx);
    auto vaapi_ctx = VAAPIContext::create(ffmpeg_ctx);
    auto sycl_ctx = SYCLContext::create(sycl_queue);
    // Create DMA context
    auto dma_ctx = DMAContext::create();
    // Create memory mapper ffmpeg->vaapi->dma->sycl
    create_mapper({vaapi_ctx, dma_ctx, sycl_ctx}, true); // 'true' in last parameter enables caching

    // Create Intel® DL Streamer transform for VAAPI-based resize and color-conversion
    TransformPtr vaapi_preproc;
    if (decoder_ctx->hw_device_ctx) {
        vaapi_preproc = create_transform(video_preproc_vaapi, {{"batch_size", 1}}, vaapi_ctx);
        FrameInfo output_info = {ImageFormat::RGBX, MemoryType::VAAPI, {TensorInfo({FLAGS_height, FLAGS_width, 1})}};
        vaapi_preproc->set_output_info(output_info);
    }

    for (;;) {
        // Read packet with compressed video frame
        AVPacket *avpacket = av_packet_alloc();
        if (av_read_frame(input_ctx, avpacket) < 0) {
            av_packet_free(&avpacket); // End of stream or error. Send NULL to avcodec_send_packet once to flush decoder
        } else if (avpacket->stream_index != video_stream) {
            av_packet_free(&avpacket); // Non-video (ex, audio) packet
            continue;
        }

        // Send packet to decoder
        DLS_CHECK_GE0(avcodec_send_packet(decoder_ctx, avpacket));

        while (true) {
            printf("---------------------------------------\n");
            // Receive frame from decoder
            auto dec_frame = av_frame_alloc();
            int decode_err = avcodec_receive_frame(decoder_ctx, dec_frame);
            if (decode_err == AVERROR(EAGAIN) || decode_err == AVERROR_EOF) {
                break;
            }
            DLS_CHECK_GE0(decode_err);

            FramePtr ffmpeg_frame = std::make_shared<FFmpegFrame>(dec_frame, true, ffmpeg_ctx); // takes ownership
            FramePtr vpp_frame = vaapi_preproc->process(ffmpeg_frame);
            FramePtr sycl_frame = vpp_frame.map(sycl_ctx);
            void *ptr = sycl_frame->tensor()->data();

            static int frame_num = 0;
            printf("Frame %d, ptr = %p\n", frame_num++, ptr);
        }

        if (avpacket)
            av_packet_free(&avpacket);
        else
            break; // End of stream
    }

    avformat_close_input(&input_ctx);

    return 0;
}
