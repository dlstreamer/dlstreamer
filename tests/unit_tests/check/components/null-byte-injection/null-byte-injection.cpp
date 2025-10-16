/*******************************************************************************
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "utils.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <iostream>

using namespace Utils;

struct NullByteInjection : public testing::Test {};

TEST_F(NullByteInjection, TestNullPathExpectFalse1) {
    EXPECT_FALSE(Utils::fileExists("../../../../etc/passwd%00"));
}

TEST_F(NullByteInjection, TestNullPathExpectFalse2) {
    EXPECT_FALSE(Utils::fileExists("%00"));
}

TEST_F(NullByteInjection, TestNullPathExpectFalse3) {
    EXPECT_FALSE(Utils::fileExists("null"));
}

TEST_F(NullByteInjection, TestNullPathExpectFalse4) {
    EXPECT_FALSE(Utils::fileExists(""));
}

TEST_F(NullByteInjection, TestNullPathExpectFalse5) {
    EXPECT_FALSE(Utils::fileExists(" "));
}

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running Components::NullByteInjection Test from " << __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}