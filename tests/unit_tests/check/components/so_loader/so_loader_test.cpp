/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "so_loader.h"

#include <gtest/gtest.h>

TEST(SOLoaderTest, load_libraries) {
    ASSERT_NO_THROW(SharedObject::getLibrary("./liblibrarymock1.so"));
    ASSERT_NO_THROW(SharedObject::getLibrary("./liblibrarymock2.so"));
    ASSERT_ANY_THROW(SharedObject::getLibrary("liblibrarymockN.so"));
    auto librarymock = SharedObject::getLibrary("./liblibrarymock1.so");
    ASSERT_TRUE(librarymock != nullptr);
}

TEST(SOLoaderTest, get_libraries_function) {
    auto librarymock = SharedObject::getLibrary("./liblibrarymock1.so");
    ASSERT_TRUE(librarymock != nullptr);
    ASSERT_NO_THROW(librarymock->getFunction<int(float)>("get42"));
    ASSERT_ANY_THROW(librarymock->getFunction<int(float)>("get24"));
    auto get42 = librarymock->getFunction<int(float)>("get42");
    ASSERT_EQ(get42(42), 42);
}

TEST(SOLoaderTest, check_singleton) {
    auto librarymock1 = SharedObject::getLibrary("./liblibrarymock1.so");
    auto librarymock2 = SharedObject::getLibrary("./liblibrarymock2.so");
    auto librarymock3 = SharedObject::getLibrary("./liblibrarymock1.so");

    ASSERT_EQ(librarymock1.get(), librarymock3.get());
    ASSERT_EQ(librarymock1.use_count(), 3);
    ASSERT_EQ(librarymock2.use_count(), 2);
}

int main(int argc, char *argv[]) {
    std::cout << "Running Components::SO Loader from " << __FILE__ << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
