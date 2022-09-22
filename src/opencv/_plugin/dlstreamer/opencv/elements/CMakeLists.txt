# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_opencv")

add_library(${TARGET_NAME} SHARED elements.cpp)
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PUBLIC
    find_contours_opencv
    object_association_opencv
    tensor_normalize_opencv
    video_cropscale_opencv
    watermark_opencv
    ${OpenCV_LIBS}
    dlstreamer_api
    remove_background_opencv
)

if (TARGET warp_affine_opencv) # available if DLS_HAVE_OPENCV_UMAT
    target_link_libraries(${TARGET_NAME} PUBLIC warp_affine_opencv)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
