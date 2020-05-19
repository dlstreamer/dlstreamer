# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

add_subdirectory(cpp/draw_face_attributes)
add_subdirectory(gst_launch/reidentification/gvaidentify)

add_custom_target(copy_model_proc ALL)
add_custom_command(TARGET copy_model_proc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/model_proc ${CMAKE_SAMPLES_OUTPUT_DIRECTORY}/model_proc)
