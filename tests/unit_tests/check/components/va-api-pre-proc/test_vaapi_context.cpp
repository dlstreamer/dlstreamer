/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/vaapi/context.h"
#include "utils.h"
#include "vaapi_context.h"

#include <gtest/gtest.h>

#include <dlstreamer/gst/context.h>
#include <memory>

using namespace InferenceBackend;

struct VAAPIContextDMATest : public testing::Test {};

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestBadInitialization) {
    ASSERT_ANY_THROW(std::unique_ptr<VaApiContext>(new VaApiContext(nullptr)));
}

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestRightInitialization) {
    VaApiDisplayPtr display = vaApiCreateVaDisplay(0);
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    ASSERT_NE(va_context, nullptr);
    auto vaapi_context = std::dynamic_pointer_cast<dlstreamer::VAAPIContext>(display);
    ASSERT_NE(vaapi_context, nullptr);
    ASSERT_EQ(vaapi_context->va_display(), va_context->DisplayRaw());
}

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestDisplay) {
    VaApiDisplayPtr display = vaApiCreateVaDisplay(0);
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    ASSERT_NO_THROW(va_context->Display());
    auto vaapi_context = std::dynamic_pointer_cast<dlstreamer::VAAPIContext>(display);
    ASSERT_NE(vaapi_context, nullptr);
    ASSERT_EQ(vaapi_context->va_display(), va_context->DisplayRaw());
}

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestRightDisplay) {
    VaApiDisplayPtr display = vaApiCreateVaDisplay(0);
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    auto val = va_context->DisplayRaw();
    ASSERT_NE(val, nullptr);
}

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestId) {
    VaApiDisplayPtr display = vaApiCreateVaDisplay(0);
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    ASSERT_NO_THROW(va_context->Id());
}

TEST_F(VAAPIContextDMATest, VAAPIContextDMATestRightId) {
    VaApiDisplayPtr display = vaApiCreateVaDisplay(0);
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    auto val = va_context->Id();
    ASSERT_NE(val, VA_INVALID_ID);
}

//==================================================================================

struct VAAPIContextVASurfaceTest : public testing::Test {
    VADisplay display = nullptr;
    int drm_fd = -1;
    VASurfaceID surface_id = -1;

    void SetUp() {
        display = createVASurface(surface_id, drm_fd);
    }
};

TEST_F(VAAPIContextVASurfaceTest, VAAPIContextVASurfaceTestInitialization) {
    ASSERT_NO_THROW(std::unique_ptr<VaApiContext>(new VaApiContext(display)));
}

TEST_F(VAAPIContextVASurfaceTest, VAAPIContextVASurfaceTestRightInitialization) {
    auto va_context = std::unique_ptr<VaApiContext>(new VaApiContext(display));
    ASSERT_NE(va_context, nullptr);
}
