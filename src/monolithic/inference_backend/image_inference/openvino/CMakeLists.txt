# ==============================================================================
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "image_inference_openvino")

find_IE_package()
find_package(ngraph REQUIRED)
find_package(PkgConfig REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )
add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for release build type
# FIXME: hence it marked as error
if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-parameter)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${InferenceEngine_LIBRARIES}
        utils
        logger
PRIVATE
        ngraph::ngraph
)

if (${ENABLE_VAAPI} AND ${InferenceEngine_VERSION} VERSION_GREATER_EQUAL "2022.1.0")
        add_definitions(-DENABLE_GPU_TILE_AFFINITY)
        target_link_libraries(${TARGET_NAME} PUBLIC va_api_wrapper)
endif()
