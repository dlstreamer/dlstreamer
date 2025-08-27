/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gflags/gflags.h>
#include <memory>
#include <thread>

// Deep Learning Streamer
#include "dlstreamer/base/blocking_queue.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/elements/ffmpeg_multi_source.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/vaapi/context.h"

// OpenVINO
#include <openvino/openvino.hpp>
#include <openvino/runtime/intel_gpu/ocl/va.hpp>
#include <openvino/runtime/intel_gpu/properties.hpp>

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

const std::string inference_device = "GPU";

DEFINE_string(i, "",
              "Required. Path to one or multiple input video files "
              "(separated by comma or delimeter specified in -delimeter option");
DEFINE_string(m, "", "Required. Path to IR .xml file");
// DEFINE_string(device, "GPU", "Device for decode and inference, 'CPU' or 'GPU'");
DEFINE_int32(batch_size, 1, "Batch size");
DEFINE_int32(nireq, 4, "Number inference requests");
DEFINE_string(delimiter, ",", "Delimiter between multiple input files in -i option, default is comma");

using namespace dlstreamer;

void print_tensor(std::vector<FramePtr> batched_frames, ov::Tensor output_tensor) {
    printf("Frames");
    for (auto &frame : batched_frames) {
        auto meta = find_metadata<SourceIdentifierMetadata>(*frame);
        if (meta)
            printf(" [stream_id=%ld, pts=%.2f]", meta->stream_id(), meta->pts() * 1e-9);
    }
    printf("\n");
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
            printf("  image%d: bbox %.2f, %.2f, %.2f, %.2f, confidence = %.5f\n", image_id, x_min, y_min, x_max, y_max,
                   confidence);
        }
    } else {
        std::cout << "  output shape=" << output_tensor.get_shape() << std::endl;
    }
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
        // Read OpenVINO™ toolkit model
        ov::Core ov_core;
        auto ov_model = ov_core.read_model(FLAGS_m);
        auto input = ov_model->inputs()[0];
        auto input_shape = input.get_shape();
        auto input_layout = ov::layout::get_layout(input);
        auto input_width = input_shape[ov::layout::width_idx(input_layout)];
        auto input_height = input_shape[ov::layout::height_idx(input_layout)];

        // Initialize FFmpeg context and ffmpeg_multi_source element (decode and resize to inference input resolution)
        auto ffmpeg_ctx = std::make_shared<FFmpegContext>(AV_HWDEVICE_TYPE_VAAPI);
        auto inputs = split_string(FLAGS_i, FLAGS_delimiter[0]);
        auto ffmpeg_source = create_source(ffmpeg_multi_source, {{"inputs", inputs}}, ffmpeg_ctx);
        TensorInfo model_input_info({input_height, input_width, 1}, DataType::UInt8);
        ffmpeg_source->set_output_info(FrameInfo(ImageFormat::NV12, MemoryType::VAAPI, {model_input_info}));

        // Configure model pre-processing
        ov::preprocess::PrePostProcessor ppp(ov_model);
        if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_VAAPI) {
            // https://docs.openvino.ai/latest/openvino_docs_OV_UG_supported_plugins_GPU_RemoteTensor_API.html#direct-nv12-video-surface-input
            ppp.input()
                .tensor()
                .set_element_type(ov::element::u8)
                .set_color_format(ov::preprocess::ColorFormat::NV12_TWO_PLANES, {"y", "uv"})
                .set_memory_type(ov::intel_gpu::memory_type::surface);
            ppp.input().preprocess().convert_color(ov::preprocess::ColorFormat::BGR);
            ppp.input().model().set_layout("NCHW");
        } else if (ffmpeg_ctx->hw_device_type() == AV_HWDEVICE_TYPE_NONE) {
            auto input_tensor_name = input.get_any_name();
            ppp.input(input_tensor_name).tensor().set_layout({"NHWC"}).set_element_type(ov::element::u8);
            ppp.input(input_tensor_name).model().set_layout(input_layout);
        } else {
            throw std::runtime_error("Unsupported hw_device_type");
        }
        ov_model = ppp.build();

        // Set batch size
        if (FLAGS_batch_size > 1)
            ov::set_batch(ov_model, FLAGS_batch_size);

        // Compile model on VAContext
        auto vaapi_ctx = VAAPIContext::create(ffmpeg_ctx);
        ov::intel_gpu::ocl::VAContext ov_context(ov_core, vaapi_ctx->va_display());
        ov::CompiledModel ov_compiled_model = ov_core.compile_model(ov_model, ov_context);

        // Create inference requests
        BlockingQueue<ov::InferRequest> free_requests;
        for (int i = 0; i < FLAGS_nireq; i++)
            free_requests.push(ov_compiled_model.create_infer_request());

        // async thread waiting for inference completion and printing inference results
        BlockingQueue<std::pair<std::vector<FramePtr>, ov::InferRequest>> busy_requests;
        std::thread thread([&] {
            for (;;) {
                auto res = busy_requests.pop();
                auto batched_frames = res.first;
                auto infer_request = res.second;
                if (!infer_request)
                    break;
                infer_request.wait();
                print_tensor(batched_frames, infer_request.get_output_tensor(0));
                free_requests.push(infer_request);
            }
            printf("print_tensor() thread completed\n");
        });

        // Frame loop
        std::vector<FramePtr> batched_frames;
        for (;;) {
            // FFmpeg video input, decode, resize
            auto frame = ffmpeg_source->read();
            if (!frame) // End-Of-Stream or error
                break;

            // fill full batch
            batched_frames.push_back(frame);
            if (batched_frames.size() < FLAGS_batch_size)
                continue;

            // zero-copy conversion from VASurfaceID to OpenVINO™ VASurfaceTensor (one tensor for Y plane, another for
            // UV)
            std::vector<ov::Tensor> y_tensors;
            std::vector<ov::Tensor> uv_tensors;
            for (auto va_frame : batched_frames) {
                VASurfaceID va_surface = ptr_cast<VAAPIFrame>(va_frame)->va_surface();
                auto nv12_tensor = ov_context.create_tensor_nv12(input_height, input_width, va_surface);
                y_tensors.push_back(nv12_tensor.first);
                uv_tensors.push_back(nv12_tensor.second);
            }

            // Get inference request and start asynchronously
            ov::InferRequest infer_request = free_requests.pop();
            infer_request.set_input_tensors(0, y_tensors);  // first input is batch of Y planes
            infer_request.set_input_tensors(1, uv_tensors); // second input is batch of UV planes
            infer_request.start_async();
            busy_requests.push({batched_frames, infer_request});

            batched_frames.clear();
        }

        // wait for all inference requests in queue
        busy_requests.push({});
        thread.join();
    } catch (const std::runtime_error &e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
