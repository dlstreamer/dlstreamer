/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_context.h"

#include "utils.h"
#include "vaapi_converter.h"

#include <gtest/gtest.h>

using namespace InferenceBackend;

struct VAAPIConverterDMATest : public testing::Test {

    MemoryType memory_type = MemoryType::DMA_BUFFER;

    std::unique_ptr<VaApiContext> va_context = nullptr;
    VADisplay display = nullptr;

    void SetUp() {
        VaApiDisplayPtr dpy = vaApiCreateVaDisplay(0);
        va_context = std::unique_ptr<VaApiContext>(new VaApiContext(dpy));
        display = va_context->DisplayRaw();
    }
};

TEST_F(VAAPIConverterDMATest, VAAPIConverterDMATestBadInitialization) {
    ASSERT_ANY_THROW(std::unique_ptr<VaApiConverter>(new VaApiConverter(nullptr)));
}

TEST_F(VAAPIConverterDMATest, VAAPIConverterDMATestInitialization) {
    ASSERT_NO_THROW(std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get())));
}

TEST_F(VAAPIConverterDMATest, VAAPIConverterDMATestRightInitialization) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));
    ASSERT_NE(va_converter, nullptr);
}

TEST_F(VAAPIConverterDMATest, VAAPIConverterDMATestBadConvert) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));

    Image src_image;
    VaApiImage dst_va_image;

    ASSERT_ANY_THROW(va_converter->Convert(src_image, dst_va_image));
}

TEST_F(VAAPIConverterDMATest, VAAPIConverterDMATestConvert) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));

    Image src_image = createEmptyImage();
    src_image.type = memory_type;
    src_image.dma_fd = -1;

    auto dst_va_image =
        std::unique_ptr<VaApiImage>(new VaApiImage(va_context.get(), 640, 480, FourCC::FOURCC_NV12, MemoryType::VAAPI));

    VaApiImage *dst_va_image_raw_ptr = dst_va_image.get();

    ASSERT_ANY_THROW(va_converter->Convert(src_image, *dst_va_image_raw_ptr));
}

//=============================================================================

struct VAAPIConverterSurfaceTest : public testing::Test {

    MemoryType memory_type = MemoryType::VAAPI;

    std::unique_ptr<VaApiContext> va_context = nullptr;
    VADisplay display = nullptr;
    int drm_fd = -1;
    VASurfaceID surface_id = -1;

    void SetUp() {
        display = createVASurface(surface_id, drm_fd);
        va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    }
};

TEST_F(VAAPIConverterSurfaceTest, VAAPIConverterSurfaceTestInitialization) {
    ASSERT_NO_THROW(std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get())));
}

TEST_F(VAAPIConverterSurfaceTest, VAAPIConverterSurfaceTestRightInitialization) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));
    ASSERT_NE(va_converter, nullptr);
}

TEST_F(VAAPIConverterSurfaceTest, VAAPIConverterSurfaceTestBadConvert) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));

    Image src_image;
    VaApiImage dst_va_image;

    ASSERT_ANY_THROW(va_converter->Convert(src_image, dst_va_image));
}

TEST_F(VAAPIConverterSurfaceTest, VAAPIConverterSurfaceTestConvert) {
    auto va_converter = std::unique_ptr<VaApiConverter>(new VaApiConverter(va_context.get()));

    int fd;
    Image src_image = createSurfaceImage(fd);

    auto dst_va_image =
        std::unique_ptr<VaApiImage>(new VaApiImage(va_context.get(), 640, 480, FourCC::FOURCC_NV12, memory_type));

    VaApiImage *dst_va_image_raw_ptr = dst_va_image.get();

    ASSERT_NO_THROW(va_converter->Convert(src_image, *dst_va_image_raw_ptr));
}
