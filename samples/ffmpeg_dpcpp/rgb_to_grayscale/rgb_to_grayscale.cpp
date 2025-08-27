/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <CL/sycl.hpp>

#include "dlstreamer/dma/context.h"
#include "dlstreamer/ffmpeg/context.h"
#include "dlstreamer/ffmpeg/elements/ffmpeg_multi_source.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/vaapi/context.h"

#include <gflags/gflags.h>
#include <memory>

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

DEFINE_string(i, "", "Required. Path to input video file");
DEFINE_string(o, "", "Required. Path to output grayscale file");
DEFINE_uint64(width, 640, "Width of output grayscale images");
DEFINE_uint64(height, 480, "Height of output grayscale images");

using namespace dlstreamer;

int main(int argc, char *argv[]) {
    if (!version_ok()) {
        std::cerr << "Header and binary mismatch for ffmpeg libav.\nPlease re-compile the sample ensuring that headers "
                     "are the same version as libraries linked by the executable."
                  << std::endl;
        return 1;
    }
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    DLS_CHECK(!FLAGS_i.empty() && !FLAGS_o.empty());
    size_t width = FLAGS_width;
    size_t height = FLAGS_height;

    for (auto platform : sycl::platform::get_platforms()) {
        std::cout << "Platform: " << platform.get_info<sycl::info::platform::name>() << std::endl;

        for (auto device : platform.get_devices()) {
            std::cout << "\tDevice: " << device.get_info<sycl::info::device::name>() << std::endl;
        }
    }

    // DPC++ queue
    sycl::queue sycl_queue = sycl::queue([](const sycl::device &dev) -> int {
        std::cout << "Device backend: " << dev.get_backend() << std::endl;
        return dev.get_backend() == sycl::backend::ext_oneapi_level_zero;
    });
    if (sycl_queue.get_backend() != sycl::backend::ext_oneapi_level_zero) {
        std::cerr
            << "No Intel® oneAPI Level Zero device found. This sample works only with Intel® oneAPI Level Zero API."
            << std::endl;
        return 1;
    }

    // Initialize FFmpeg context and ffmpeg_multi_source element (decode and resize to specified resolution)
    auto ffmpeg_ctx = std::make_shared<FFmpegContext>(AV_HWDEVICE_TYPE_VAAPI);
    std::vector<std::string> inputs = {FLAGS_i};
    auto ffmpeg_source = create_source(ffmpeg_multi_source, {{"inputs", inputs}}, ffmpeg_ctx);
    FrameInfo output_info = {ImageFormat::RGBX, MemoryType::VAAPI, {TensorInfo({height, width, 1})}};
    ffmpeg_source->set_output_info(output_info);

    // Create Deep Learning Streamer context objects for VAAPI, SYCL and DMA
    auto vaapi_ctx = ffmpeg_source->get_context(MemoryType::VAAPI);
    auto sycl_ctx = SYCLContext::create(sycl_queue);
    auto dma_ctx = DMAContext::create();
    // Create memory mapper ffmpeg->vaapi->dma->sycl
    create_mapper({vaapi_ctx, dma_ctx, sycl_ctx}, true); // 'true' in last parameter enables caching

    // Open output file (grayscale data)
    FILE *file = fopen(FLAGS_o.data(), "wb");
    if (!file) {
        printf("Error creating file %s\n", FLAGS_o.data());
        return -1;
    }

    uint8_t *gray_data = sycl::malloc_shared<uint8_t>(height * width, sycl_queue);

    // frame loop
    std::vector<FramePtr> frames;
    for (;;) {
        // FFmpeg video input, decode, resize, color-conversion
        auto frame = ffmpeg_source->read();
        if (!frame) // End-Of-Stream or error
            break;

        FramePtr sycl_frame = frame.map(sycl_ctx);
        uint8_t *device_ptr = (uint8_t *)sycl_frame->tensor()->data();
        ImageInfo image_info(sycl_frame->tensor()->info());
        auto stride = image_info.width_stride();

        static int frame_num = 0;
        printf("Frame %d, device ptr = %p\n", frame_num++, device_ptr);

        sycl_queue
            .parallel_for(sycl::range<2>(height, width),
                          [=](sycl::item<2> item) {
                              size_t y = item[0];
                              size_t x = item[1];
                              uint8_t *pixel = device_ptr + y * stride + 4 * x;
                              uint8_t gray = (uint8_t)(0.2126f * pixel[2] + 0.7152f * pixel[1] + 0.0722f * pixel[0]);
                              gray_data[y * width + x] = gray;
                          })
            .wait();

        fwrite(gray_data, 1, height * width, file);
    }

    sycl::free(gray_data, sycl_queue);
    fclose(file);
    printf("\nCreated file %s\n", FLAGS_o.data());

    return 0;
}
