# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.16)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED IMPORTED_TARGET
  gstreamer-1.0
  gstreamer-base-1.0
  gstreamer-video-1.0
)

# GstVA (the 'va' plugin helper library) – skip on MSVC builds (Windows) where VAAPI isn't available
if(NOT MSVC)
  pkg_check_modules(GSTVA REQUIRED IMPORTED_TARGET gstreamer-va-1.0)
endif()

# Renamed target and output (old: gstva_gpu_filter / va_gpu_filter)
# Add required post-processing implementation sources for ROIToFrameAttacher vtable
if(MSVC)
  # Minimal source set for Windows (avoid OpenVINO-dependent converters)
  set(GVAMD_EXTRA_SOURCES
    # Strip analytics/meta attachment & converters to avoid unresolved externals
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/perf_logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/utils.cpp
  )
else()
  set(GVAMD_EXTRA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/meta_attacher.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/frame_wrapper.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/post_proc_common.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/blob_to_meta_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/common/gva_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/perf_logger.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/clip_token_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/keypoints_3d.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/keypoints_hrnet.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/keypoints_openpose.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/label.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/paddle_ocr.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/raw_data_copy.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/text.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/blob_to_tensor_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/custom_to_tensor.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/detection_anomaly.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/semantic_mask.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/runtime_feature_toggling/runtime_feature_toggler.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/human_pose_extractor/human_pose_extractor.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/human_pose_extractor/peak.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_tensor/docTR_ocr.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/base/copy_blob_to_gststruct.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/utils.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/blob_to_roi_converter.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/boxes_labels.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/detection_output.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/mask_rcnn.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v2.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v3.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v8.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/boxes_labels_scores_base.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/boxes_scores.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/centerface.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/custom_to_roi.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/heatmap_boxes.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_base.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v4.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v5.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v7.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_v10.cpp
    ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor/converters/to_roi/yolo_x.cpp
  )
endif()

if(MSVC OR WIN32)
  set(GVAMD_MAIN_SOURCE gvamotiondetect_win.cpp)
else()
  set(GVAMD_MAIN_SOURCE gvamotiondetect.cpp)
endif()

add_library(gstgvamotiondetect MODULE
  ${GVAMD_MAIN_SOURCE}
  ${GVAMD_EXTRA_SOURCES}
)

set_target_properties(gstgvamotiondetect PROPERTIES
  OUTPUT_NAME "gvamotiondetect"    # libgstgvamotiondetect.so
  PREFIX "libgst"
)

# Disable ITT notify API to avoid linking against external instrumentation library
target_compile_definitions(gstgvamotiondetect PRIVATE INTEL_NO_ITTNOTIFY_API)

target_include_directories(gstgvamotiondetect PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${GST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/post_processor
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/common
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/base
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/runtime_feature_toggling/
  ${CMAKE_SOURCE_DIR}/src/gst/bins/model_proc
  ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/include/
  ${CMAKE_SOURCE_DIR}/src/utils/
  ${CMAKE_SOURCE_DIR}/src/monolithic/gst/inference_elements/common/
  ${CMAKE_SOURCE_DIR}/include/dlstreamer/gst/videoanalytics/
  ${CMAKE_SOURCE_DIR}/thirdparty/itt/include
  )

target_link_libraries(gstgvamotiondetect PRIVATE
  PkgConfig::GST
  dlstreamer_gst
  dlstreamer_gst_meta
)

if(NOT MSVC)
  # VAAPI / GstVA only for non-Windows builds
  target_link_libraries(gstgvamotiondetect PRIVATE
    PkgConfig::GSTVA
    dlstreamer_vaapi
  )
endif()

if(NOT MSVC)
  pkg_check_modules(LIBVA QUIET IMPORTED_TARGET libva)
  if (LIBVA_FOUND)
    target_link_libraries(gstgvamotiondetect PRIVATE PkgConfig::LIBVA)
  else()
    find_library(LIBVA_LIB va)
    if (LIBVA_LIB)
      target_link_libraries(gstgvamotiondetect PRIVATE ${LIBVA_LIB})
    endif()
  endif()
endif()

find_package(OpenCL QUIET)
if (OpenCL_FOUND)
  target_link_libraries(gstgvamotiondetect PRIVATE OpenCL::OpenCL)
  target_compile_definitions(gstgvamotiondetect PRIVATE HAVE_OPENCL_LOADER=1)
endif()

find_package(OpenCV QUIET COMPONENTS core imgproc)
if (OpenCV_FOUND)
  target_include_directories(gstgvamotiondetect PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(gstgvamotiondetect PRIVATE ${OpenCV_LIBS})
  target_compile_definitions(gstgvamotiondetect PRIVATE HAVE_OPENCV=1)
endif()

install(TARGETS gstgvamotiondetect
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0
)
