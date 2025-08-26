# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

include(ExternalProject)

# When changing version, you will also need to change the download hash
set(DESIRED_VERSION 6.1.1)

# Verify the version of ffmpeg libraries over at https://ffmpeg.org/download.html
find_package(PkgConfig)
pkg_check_modules(LIBAV libavformat>=60.16.100 libavcodec>=60.31.102 libswscale>=7.5.100 libavutil>=58.29.100)

if (LIBAV_FOUND)
    return()
endif()

ExternalProject_Add(
    ffmpeg
    PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
    URL     https://ffmpeg.org/releases/ffmpeg-${DESIRED_VERSION}.tar.gz
    URL_MD5 cce359cad7ed0d4f0079f7864080ad36
    INSTALL_COMMAND make install
    TEST_COMMAND    ""
    CONFIGURE_COMMAND   <SOURCE_DIR>/configure 
                        --enable-pic 
                        --enable-shared 
                        --enable-static 
                        --enable-avfilter 
                        --enable-vaapi 
                        --extra-cflags=-I/include 
                        --extra-ldflags=-L/lib 
                        --extra-libs=-lpthread 
                        --extra-libs=-lm 
                        --disable-programs
                        --prefix==${CMAKE_BINARY_DIR}/install 
)
