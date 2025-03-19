# ==============================================================================
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package (Threads)

set(TARGET_NAME "ittnotify")

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME} INTERFACE include)
target_link_libraries(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libittnotify.a ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
