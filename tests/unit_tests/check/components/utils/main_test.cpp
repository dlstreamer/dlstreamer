/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gtest/gtest.h>

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running Components::utils Test from " << __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    // gst_check_init(&argc, &argv);

    return RUN_ALL_TESTS();
}
