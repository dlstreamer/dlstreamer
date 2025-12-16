# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_opencv")
set(SRC elements.cpp)
set(LINK_LIBS
    opencv_barcode_detector
    opencv_find_contours
    opencv_object_association
    opencv_tensor_normalize
    opencv_cropscale
    opencv_meta_overlay
    ${OpenCV_LIBS}
    dlstreamer_api
    opencv_remove_background
    tensor_postproc_human_pose
)

function(add_dlstreamer_target type suffix)
    add_library(${TARGET_NAME}${suffix} ${type} ${SRC})
    set_compile_flags(${TARGET_NAME}${suffix})
    target_link_libraries(${TARGET_NAME}${suffix} PUBLIC ${LINK_LIBS})
    if (TARGET opencv_warp_affine)
        target_link_libraries(${TARGET_NAME}${suffix} PUBLIC opencv_warp_affine)
    endif()
    set_target_properties(${TARGET_NAME}${suffix} PROPERTIES OUTPUT_NAME ${TARGET_NAME})
    install(TARGETS ${TARGET_NAME}${suffix} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
endfunction()

add_dlstreamer_target("SHARED" "")
add_dlstreamer_target("STATIC" "_static")