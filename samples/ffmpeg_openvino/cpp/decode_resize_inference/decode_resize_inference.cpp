/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/elements/multi_source_ffmpeg.h"
#include "dlstreamer/ffmpeg/frame.h"
#include "dlstreamer/openvino/context.h"
#include "dlstreamer/transform.h"
#include "dlstreamer/vaapi/context.h"
#include "dlstreamer/vaapi/elements/video_preproc_vaapi.h"

#include <gflags/gflags.h>
#include <memory>

#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/va.hpp>

extern "C" {
// FFmpeg
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

DEFINE_string(i, "", "Required. Path to input video file");
DEFINE_string(m, "", "Required. Path to IR .xml file");
DEFINE_string(device, "GPU", "Device for decode and inference, 'CPU' or 'GPU'");
DEFINE_int32(batch_size, 1, "Confidence threshold for bounding boxes, in range [0-1]");

using namespace dlstreamer;

int main(int argc, char *argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    DLS_CHECK(!FLAGS_i.empty() && !FLAGS_m.empty());

    auto source_ffmpeg = create_source(multi_source_ffmpeg, {{"input", FLAGS_i}});
    auto ffmpeg_ctx = ptr_cast<FFmpegContext>(source_ffmpeg->get_context(MemoryType::FFmpeg));

    // Load OpenVINO model
    ov::Core ov_core;
    auto ov_model = ov_core.read_model(FLAGS_m);
    auto input = ov_model->inputs()[0];
    auto input_tensor_name = input.get_any_name();
    const ov::Shape &input_shape = input.get_shape();
    ov::Layout inputLayout = ov::layout::get_layout(input);
    ov::preprocess::PrePostProcessor ppp(ov_model);
    if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_NONE) {
        ppp.input(input_tensor_name).tensor().set_layout({"NHWC"}).set_element_type(ov::element::u8);
        ppp.input(input_tensor_name).model().set_layout(inputLayout);
    } else if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_VAAPI) {
        // https://docs.openvino.ai/latest/openvino_docs_OV_UG_supported_plugins_GPU_RemoteTensor_API.html#direct-nv12-video-surface-input
        ppp.input()
            .tensor()
            .set_element_type(ov::element::u8)
            .set_color_format(ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"})
            .set_memory_type(ov::intel_gpu::memory_type::surface);
        ppp.input().preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
        ppp.input().model().set_layout("NCHW");
    } else {
        throw std::runtime_error("Unsupported hw_device_type");
    }
    auto input_width = input_shape[ov::layout::width_idx(inputLayout)];
    auto input_height = input_shape[ov::layout::height_idx(inputLayout)];
    ov_model = ppp.build();
    if (FLAGS_batch_size > 1)
        ov::set_batch(ov_model, FLAGS_batch_size);

    // Compile model and create inference request
    ov::CompiledModel ov_compiled_model;
    ContextPtr vaapi_ctx;
    OpenVINOContextPtr openvino_ctx;
    if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_VAAPI) {
        vaapi_ctx = VAAPIContext::create(ffmpeg_ctx);
        openvino_ctx = std::make_shared<OpenVINOContext>(ov_core, FLAGS_device, vaapi_ctx);
        ov_compiled_model = ov_core.compile_model(ov_model, *openvino_ctx);
    } else {
        openvino_ctx = std::make_shared<OpenVINOContext>();
        ov_compiled_model = ov_core.compile_model(ov_model, FLAGS_device);
    }
    ov::InferRequest infer_request = ov_compiled_model.create_infer_request();

    // GPU-based resize and color-conversion - create DL Streamer element 'video_preproc_vaapi'
    TransformPtr vaapi_preproc;
    if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_VAAPI) {
        vaapi_preproc = create_transform(video_preproc_vaapi, {}, vaapi_ctx);
        FrameInfo output_info = {ImageFormat::NV12, MemoryType::VAAPI, {TensorInfo({input_height, input_width, 1})}};
        vaapi_preproc->set_output_info(output_info);
    }

    std::vector<FramePtr> resized_frames;
    for (;;) {
        // FFmpeg video input and video decode
        auto frame = source_ffmpeg->read();
        if (!frame) // End Of Streamer or error
            break;

        // GPU resize
        frame = vaapi_preproc->process(frame);

        // fill full batch
        resized_frames.push_back(frame);
        if (resized_frames.size() < FLAGS_batch_size)
            continue;

        std::vector<ov::Tensor> y_tensors;
        std::vector<ov::Tensor> uv_tensors;
        if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_VAAPI) {
            for (auto resized_frame : resized_frames) {
                VASurfaceID vpp_va_surface = ptr_cast<VAAPIFrame>(resized_frame)->va_surface();
                printf("vpp_va_surface %d\n", vpp_va_surface);
                // zero-copy conversion from VAAPI surface to OpenVINO tensors (one tensor for Y plane, another for UV)
                auto nv12_tensor = openvino_ctx->remote_context<ov::intel_gpu::ocl::VAContext>().create_tensor_nv12(
                    input_height, input_width, vpp_va_surface);
                y_tensors.push_back(nv12_tensor.first);
                uv_tensors.push_back(nv12_tensor.second);
            }
            // infer_request.set_tensors(input_tensor_name + "/y", y_tensors);
            // infer_request.set_tensors(input_tensor_name + "/uv", uv_tensors);
        } else { // CPU resize
            throw std::runtime_error("TODO");
#if 0
                AVFrame *av_frame = av_frame_alloc();
                av_frame->format = AV_PIX_FMT_BGR24;
                av_frame->width = input_width;
                av_frame->height = input_height;
                DLS_CHECK_GE0(av_frame_get_buffer(av_frame, 0));
                if (!sws_context) {
                    sws_context =
                        sws_getContext(dec_frame->width, dec_frame->height,
                                       static_cast<AVPixelFormat>(dec_frame->format), av_frame->width, av_frame->height,
                                       static_cast<AVPixelFormat>(av_frame->format), SWS_BICUBIC, NULL, NULL, NULL);
                    DLS_CHECK(sws_context);
                }
                DLS_CHECK(sws_scale(sws_context, dec_frame->data, dec_frame->linesize, 0, dec_frame->height,
                                    av_frame->data, av_frame->linesize));

                // free decoded frame
                av_frame_free(&dec_frame);

                // zero-copy mapping from FFmpeg to OpenVINO
                resized_frames.push_back(std::make_shared<FFmpegFrame>(av_frame, true));
                TensorPtr openvino_tensor = resized_frames.front().map(openvino_ctx)->tensor();
                infer_request.set_input_tensor(0, *ptr_cast<OpenVINOTensor>(openvino_tensor));
#endif
        }

        // Run inference request
        infer_request.set_tensors(input_tensor_name + "/y", y_tensors);
        infer_request.set_tensors(input_tensor_name + "/uv", uv_tensors);
        infer_request.infer();

        auto output_tensor = infer_request.get_output_tensor(0);

        // If object detection model, print bounding box coordinates and confidence. Otherwise, print output shape
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
                printf("image%d: bbox %.2f, %.2f, %.2f, %.2f, confidence = %.5f\n", image_id, x_min, y_min, x_max,
                       y_max, confidence);
            }
        } else {
            std::cout << "output shape=" << output_tensor.get_shape() << std::endl;
        }

        resized_frames.clear();
    }

    return 0;
}
