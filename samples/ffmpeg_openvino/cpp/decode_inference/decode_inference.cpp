/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <cassert>
#include <gflags/gflags.h>
#include <memory>

// OpenVINO
#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/va.hpp>
#include <openvino/runtime/intel_gpu/properties.hpp>

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>
}

auto constexpr inference_device = "GPU";

DEFINE_string(i, "", "Required. Path to input video file");
DEFINE_string(m, "", "Required. Path to IR .xml file");
// DEFINE_string(device, "GPU", "Device for decode and inference, 'CPU' or 'GPU'");

#define DLS_CHECK(_VAR)                                                                                                \
    if (!(_VAR))                                                                                                       \
        throw std::runtime_error(std::string(__FUNCTION__) + ": Error on: " #_VAR);

#define DLS_CHECK_GE0(_VAR)                                                                                            \
    {                                                                                                                  \
        auto _res = _VAR;                                                                                              \
        if (_res < 0)                                                                                                  \
            throw std::runtime_error(std::string(__FUNCTION__) + ": Error " + std::to_string(_res) +                   \
                                     " calling: " #_VAR);                                                              \
    }

std::string version_to_string(int version) {
    return std::to_string(AV_VERSION_MAJOR(version)) + "." + std::to_string(AV_VERSION_MINOR(version)) + "." +
           std::to_string(AV_VERSION_MICRO(version));
}

bool version_check(int binary_version, int header_version, const char *name) {
    if (AV_VERSION_MAJOR(binary_version) != AV_VERSION_MAJOR(header_version)) {
        std::cerr << "Warning: " << name << " ABI mismatch! Library version: " << version_to_string(binary_version)
                  << " header version: " << version_to_string(header_version) << std::endl;
        return false;
    }
    return true;
}

bool version_ok() {
    return version_check(avformat_version(), LIBAVFORMAT_VERSION_INT, "avformat") &&
           version_check(avcodec_version(), LIBAVCODEC_VERSION_INT, "avcodec") &&
           version_check(avutil_version(), LIBAVUTIL_VERSION_INT, "avutil");
}

int main(int argc, char *argv[]) {
    if (!version_ok()) {
        std::cerr << "Header and binary mismatch for ffmpeg libav.\nPlease re-compile the sample ensuring that headers "
                     "are the same version as libraries linked by the executable."
                  << std::endl;
        return 1;
    }
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_i.empty() || FLAGS_m.empty()) {
        std::cerr << "Required command line arguments were not set: -i input_video.mp4 -m model_file.xml" << std::endl;
        return 1;
    }

    try {
        // find video stream information
        AVInputFormat *input_format = NULL;
        AVFormatContext *input_ctx = NULL;
        DLS_CHECK_GE0(avformat_open_input(&input_ctx, FLAGS_i.data(), input_format, NULL));
        const AVCodec *codec = nullptr;
        int video_stream = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        DLS_CHECK_GE0(video_stream);
        AVCodecParameters *codecpar = input_ctx->streams[video_stream]->codecpar;

        // create FFmpeg VAAPI decoder
        AVCodecContext *decoder_ctx = NULL;
        DLS_CHECK(decoder_ctx = avcodec_alloc_context3(codec));
        DLS_CHECK_GE0(avcodec_parameters_to_context(decoder_ctx, codecpar));
        const char *device = nullptr;
        DLS_CHECK_GE0(av_hwdevice_ctx_create(&decoder_ctx->hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, device, NULL, 0));
        decoder_ctx->get_format = [](AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
            return AV_PIX_FMT_VAAPI; // request VAAPI frame format
        };
        DLS_CHECK_GE0(avcodec_open2(decoder_ctx, codec, NULL));

        // Read OpenVINO™ toolkit model
        ov::Core ov_core;
        auto ov_model = ov_core.read_model(FLAGS_m);
        auto input = ov_model->input();
        auto input_tensor_name = input.get_any_name();

        // Reshape model to NCHW (planar BGR) layout and width/height reported by ffmpeg
        ov_model->reshape({1, 3, codecpar->height, codecpar->width});
        ov::Shape input_shape = input.get_shape();
        ov::Layout inputLayout = ov::layout::get_layout(input);
        auto input_width = input_shape[ov::layout::width_idx(inputLayout)];
        auto input_height = input_shape[ov::layout::height_idx(inputLayout)];

        // Configure model pre-processing
        ov::preprocess::PrePostProcessor ppp(ov_model);
        if (decoder_ctx->hw_device_ctx) { // VAAPI decode
            // https://docs.openvino.ai/latest/openvino_docs_OV_UG_supported_plugins_GPU_RemoteTensor_API.html#direct-nv12-video-surface-input
            ppp.input()
                .tensor()
                .set_element_type(ov::element::u8)
                .set_color_format(ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"})
                .set_memory_type(ov::intel_gpu::memory_type::surface);
            ppp.input().preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
            ppp.input().model().set_layout("NCHW");
        } else { // CPU decode
            ppp.input(input_tensor_name).tensor().set_layout({"NHWC"}).set_element_type(ov::element::u8);
            ppp.input(input_tensor_name).model().set_layout(inputLayout);
        }
        ov_model = ppp.build();

        // Compile model and create inference request
        ov::CompiledModel ov_compiled_model;
        if (decoder_ctx->hw_device_ctx) {
            AVHWDeviceContext *device_ctx = (AVHWDeviceContext *)decoder_ctx->hw_device_ctx->data;
            assert(device_ctx->type == AV_HWDEVICE_TYPE_VAAPI);
            AVVAAPIDeviceContext *vaapi_ctx = (AVVAAPIDeviceContext *)device_ctx->hwctx;
            ov::intel_gpu::ocl::VAContext ov_context(ov_core, vaapi_ctx->display);
            ov_compiled_model = ov_core.compile_model(ov_model, ov_context);
        } else {
            ov_compiled_model = ov_core.compile_model(ov_model, inference_device);
        }
        ov::InferRequest infer_request = ov_compiled_model.create_infer_request();

        for (;;) {
            // Read packet with compressed video frame
            AVPacket *avpacket = av_packet_alloc();
            if (av_read_frame(input_ctx, avpacket) < 0) {
                av_packet_free(
                    &avpacket); // End of stream or error. Send NULL to avcodec_send_packet once to flush decoder
            } else if (avpacket->stream_index != video_stream) {
                av_packet_free(&avpacket); // Non-video (ex, audio) packet
                continue;
            }

            // Send packet to decoder
            DLS_CHECK_GE0(avcodec_send_packet(decoder_ctx, avpacket));

            // Receive frame(s) from decoder
            while (true) {
                auto av_frame = av_frame_alloc();
                int decode_err = avcodec_receive_frame(decoder_ctx, av_frame);
                if (decode_err == AVERROR(EAGAIN) || decode_err == AVERROR_EOF) {
                    break;
                }
                DLS_CHECK_GE0(decode_err);
                static int frame_num = 0;
                printf("Frame %d\n", frame_num++);

                std::vector<ov::Tensor> y_tensors;
                std::vector<ov::Tensor> uv_tensors;
                if (av_frame->format != AV_PIX_FMT_VAAPI)
                    throw std::runtime_error("Unsupported av_frame format");
                VASurfaceID va_surface = (VASurfaceID)(size_t)av_frame->data[3]; // As defined by AV_PIX_FMT_VAAPI
                // printf("va_surface %d\n", va_surface);
                // zero-copy conversion from VAAPI surface to OpenVINO™ toolkit tensors (one for Y plane, another for
                // UV)
                auto va_context = ov_compiled_model.get_context().as<ov::intel_gpu::ocl::VAContext>();
                auto nv12_tensor = va_context.create_tensor_nv12(input_height, input_width, va_surface);

                // Run inference request
                infer_request.set_input_tensor(0, nv12_tensor.first);  // Y plane
                infer_request.set_input_tensor(1, nv12_tensor.second); // UV plane
                infer_request.infer();
                auto output_tensor = infer_request.get_output_tensor(0);

                // If object detection model, print bounding box coordinates and confidence. Otherwise, print output
                // shape
                size_t last_dim = output_tensor.get_shape().back();
                if (last_dim == 7) {
                    // suppose object detection model with output [image_id, label_id, confidence, bbox coordinates]
                    float *data = (float *)output_tensor.data();
                    for (size_t i = 0; i < output_tensor.get_size() / last_dim; i++) {
                        int image_id = static_cast<int>(data[i * last_dim + 0]);
                        float confidence = data[i * last_dim + 2];
                        float x_min = data[i * last_dim + 3];
                        float y_min = data[i * last_dim + 4];
                        float x_max = data[i * last_dim + 5];
                        float y_max = data[i * last_dim + 6];
                        if (image_id < 0)
                            break;
                        if (confidence < 0.5) {
                            continue;
                        }
                        printf("  bbox %.2f, %.2f, %.2f, %.2f, confidence = %.5f\n", x_min, y_min, x_max, y_max,
                               confidence);
                    }
                } else {
                    std::cout << "output shape=" << output_tensor.get_shape() << std::endl;
                }

                av_frame_free(&av_frame);
            }

            if (avpacket)
                av_packet_free(&avpacket);
            else
                break; // End of stream
        }

        avformat_close_input(&input_ctx);
    } catch (const std::runtime_error &err) {
        std::cerr << "Runtime error " << err.what() << std::endl;
        return 1;
    }
    return 0;
}
