#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

MODEL1_PATH=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml
MODEL2_PATH=${MODELS_PATH}/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml
MODEL3_PATH=${MODELS_PATH}/intel/emotions-recognition-retail-0003/FP32/emotions-recognition-retail-0003.xml
MODEL4_PATH=${MODELS_PATH}/intel/landmarks-regression-retail-0009/FP32/landmarks-regression-retail-0009.xml

gst-launch-1.0 \
$SOURCE_ELEMENT ! \
decodebin force-sw-decoders=true ! \
processbin \
    preprocess="videoscale ! videoconvert ! video/x-raw,format=BGRP ! tensor_convert" \
    process="openvino_tensor_inference model=$MODEL1_PATH device=CPU" \
    postprocess="tensor_postproc_detection threshold=0.5" \
    aggregate="meta_aggregate attach-tensor-data=false" ! \
processbin \
    preprocess="roi_split ! videoconvert ! opencv_cropscale ! videoconvert ! video/x-raw,format=BGRP ! tensor_convert" \
    process="openvino_tensor_inference model=$MODEL2_PATH device=CPU" \
    postprocess="tensor_postproc_label layer-name=prob attribute-name=gender labels=<Female,Male> ! tensor_postproc_text layer-name=age_conv3 text-scale=100" \
    aggregate=meta_aggregate ! \
processbin \
    preprocess="roi_split ! videoconvert ! opencv_cropscale ! videoconvert ! video/x-raw,format=BGRP ! tensor_convert" \
    process="openvino_tensor_inference model=$MODEL3_PATH device=CPU" \
    postprocess="tensor_postproc_label labels=<neutral,happy,sad,surprise,anger> attribute-name=emotion method=max" \
    aggregate=meta_aggregate ! \
processbin \
    preprocess="roi_split ! videoconvert ! opencv_cropscale ! videoconvert ! video/x-raw,format=BGRP ! tensor_convert" \
    process="openvino_tensor_inference model=$MODEL4_PATH device=CPU" \
    postprocess="tensor_postproc_add_params format=landmark_points" \
    aggregate=meta_aggregate ! \
gvawatermark ! \
videoconvert ! \
gvafpscounter ! \
autovideosink sync=false
