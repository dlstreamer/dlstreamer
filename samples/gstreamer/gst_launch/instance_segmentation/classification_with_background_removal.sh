#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/bottle-detection.mp4}
DEVICE=${2:-CPU}
OUTPUT=${3:-display} # Supported values: display, fps, json, display-and-json
SEGMENTATION_MODEL=${4:-instance-segmentation-security-1040}

SCRIPTDIR="$(dirname "$(realpath "$0")")"
PYTHON_SCRIPT=$SCRIPTDIR/tensor_to_box_mask.py

if [[ $OUTPUT == "display" ]] || [[ -z $OUTPUT ]]; then
  SINK_ELEMENT="find_contours_opencv ! gvawatermark ! videoconvert ! gvafpscounter ! autovideosink sync=false"
elif [[ $OUTPUT == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false "
elif [[ $OUTPUT == "json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! fakesink async=false "
elif [[ $OUTPUT == "display-and-json" ]]; then
  rm -f output.json
  SINK_ELEMENT="find_contours_opencv ! gvawatermark ! gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! videoconvert ! gvafpscounter ! autovideosink sync=false"
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Valid values: "display" - render to screen, "fps" - print FPS, "json" - write to output.json, "display-and-json" - render to screen and write to output.json
  exit
fi

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

MODEL_PATH=${MODELS_PATH}/intel/${SEGMENTATION_MODEL}/FP32/${SEGMENTATION_MODEL}.xml
CLASSIFICATION_MODEL_PATH=${MODELS_PATH}/public/efficientnet-b0/FP32/efficientnet-b0.xml

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}

LABELS_PATH=$(dirname "$0")/../../../labels

PIPELINE="gst-launch-1.0 \
$SOURCE_ELEMENT ! decodebin force-sw-decoders=true ! \
video_detect \
  model=$MODEL_PATH device=$DEVICE \
  labels-file=$(realpath $LABELS_PATH/coco_80cl.txt) ! \
video_classify \
  preprocess='videoconvert ! roi_split ! video_cropscale_opencv ! remove_background_opencv ! \
    videoconvert ! videoscale ! tensor_convert' \
  model=$CLASSIFICATION_MODEL_PATH device=$DEVICE \
  postprocess='tensor_postproc_label threshold=0.2 \
    labels-file=$(realpath $LABELS_PATH/imagenet_2012.txt) method=softmax ' ! \
$SINK_ELEMENT"
echo ${PIPELINE}
$PIPELINE
