/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running Components::VAAPIPreProc Test from " << __FILE__ << std::endl;
    try {
        testing::InitGoogleTest(&argc, argv);
        // gst_check_init(&argc, &argv);

        return RUN_ALL_TESTS();
    } catch (const std::exception &e) {
        std::cerr << "Caught std::exception in " << __FILE__ << " at line " << __LINE__ << " in function "
                  << __FUNCTION__ << std::endl;
        std::cerr << "Context: Failed during GoogleTest initialization: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Caught unknown exception in " << __FILE__ << " at line " << __LINE__ << " in function "
                  << __FUNCTION__ << std::endl;
        std::cerr << "Context: Failed during GoogleTest initialization with unknown exception type" << std::endl;
        return 1;
    }
}
