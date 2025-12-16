# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_opencl")
set(SOURCES elements.cpp)
set(LINK_LIBS opencl_tensor_normalize)

if(VA_FOUND)
    list(APPEND LINK_LIBS vaapi_to_opencl)
endif()

foreach(TYPE SHARED STATIC)
    set(LIB_NAME ${TARGET_NAME})
    if(TYPE STREQUAL "STATIC")
        set(LIB_NAME ${TARGET_NAME}_static)
    endif()

    add_library(${LIB_NAME} ${TYPE} ${SOURCES})
    set_compile_flags(${LIB_NAME})
    target_link_libraries(${LIB_NAME} PUBLIC ${LINK_LIBS})
    set_target_properties(${LIB_NAME} PROPERTIES OUTPUT_NAME ${TARGET_NAME})
    install(TARGETS ${LIB_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
endforeach()