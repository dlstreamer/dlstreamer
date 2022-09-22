#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e
set -x

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}
DEVICE=${2:-CPU}   # Inference device
OUTPUT=${3:-display-async}     # Output type, valid values: display, display-async, fps, json, display-and-json
MODEL=${4:-${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml}

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

if [[ $OUTPUT == "display" ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! gvafpscounter ! autovideosink"
elif [[ $OUTPUT == "display-async" ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! gvafpscounter ! autovideosink sync=false"
elif [[ $OUTPUT == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false "
elif [[ $OUTPUT == "json" ]]; then
  SINK_ELEMENT="gvametaconvert ! gvametapublish file-format=json-lines file-path=output.json ! fakesink async=false"
elif [[ $OUTPUT == "display-and-json" ]]; then
  SINK_ELEMENT="gvawatermark ! gvametaconvert ! gvametapublish file-format=json-lines file-path=output.json ! videoconvert ! gvafpscounter ! autovideosink sync=false"
else
  echo Error wrong value for OUTPUT parameter
  echo Valid values: "display" - render to screen, "fps" - print FPS, "json" - write to output.json, "display-and-json" - render to screen and write to output.json
  exit
fi

# add this folder to GST_PLUGIN_PATH to register plugin in sub-folder 'python'
GST_PLUGIN_PATH=$(dirname "$0"):$GST_PLUGIN_PATH \
gst-launch-1.0 \
    $SOURCE_ELEMENT ! \
    decodebin ! \
    "video/x-raw(memory:VASurface)" ! \
    splitjoinbin \
        preprocess="vaapipostproc ! videoconvert ! tensor_convert" \
        process="inference_openvino model=$MODEL device=$DEVICE" \
        postprocess=tensor_postproc_detection_output \
        aggregate=meta_aggregate ! \
    vaapipostproc ! \
    $SINK_ELEMENT
