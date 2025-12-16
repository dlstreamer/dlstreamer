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

struct RegExpression : public testing::Test {};

TEST_F(RegExpression, TestInvalidCharacterInRegExpExpectFalse1) {
    EXPECT_FALSE(Utils::fileExists("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa!"));
}

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running Components::RegularExpression Test from " << __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}