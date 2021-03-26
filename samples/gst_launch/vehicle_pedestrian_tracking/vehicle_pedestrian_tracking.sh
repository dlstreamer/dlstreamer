#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

# input parameters
FILE=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/person-bicycle-car-detection.mp4}

DETECTION_INTERVAL=${2:-10}

INFERENCE_DEVICE=${3:-CPU}

if [[ $4 == "display" ]] || [[ -z $4 ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"
elif [[ $4 == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false "
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Possible values: display - render, fps - show FPS only
  exit
fi

MODEL_1=person-vehicle-bike-detection-crossroad-0078
MODEL_2=person-attributes-recognition-crossroad-0230
MODEL_3=vehicle-attributes-recognition-barrier-0039

# Other tracking types available are short-term-imageless, zero-term, zero-term-imageless.
# For more information on tracking types and their difference, please turn to
# https://github.com/openvinotoolkit/dlstreamer_gst/wiki/Object-tracking.
TRACKING_TYPE="short-term"

RECLASSIFY_INTERVAL=10

if [[ $FILE == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${FILE}"
elif [[ $FILE == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${FILE}"
else
  SOURCE_ELEMENT="filesrc location=${FILE}"
fi

PROC_PATH() {
    echo $(dirname "$0")/model_proc/$1.json
}

DETECTION_MODEL=${MODELS_PATH}/intel/person-vehicle-bike-detection-crossroad-0078/FP32/person-vehicle-bike-detection-crossroad-0078.xml 
PERSON_CLASSIFICATION_MODEL=${MODELS_PATH}/intel/person-attributes-recognition-crossroad-0230/FP32/person-attributes-recognition-crossroad-0230.xml
VEHICLE_CLASSIFICATION_MODEL=${MODELS_PATH}/intel/vehicle-attributes-recognition-barrier-0039/FP32/vehicle-attributes-recognition-barrier-0039.xml

DETECTION_MODEL_PROC=$(PROC_PATH $MODEL_1)
PERSON_CLASSIFICATION_MODEL_PROC=$(PROC_PATH $MODEL_2)
VEHICLE_CLASSIFICATION_MODEL_PROC=$(PROC_PATH $MODEL_3)

PIPELINE="gst-launch-1.0 \
  ${SOURCE_ELEMENT} ! decodebin ! \
  gvadetect model=$DETECTION_MODEL \
            model-proc=$DETECTION_MODEL_PROC \
            inference-interval=${DETECTION_INTERVAL} \
            threshold=0.6 \
            device=${INFERENCE_DEVICE} ! \
  queue ! \
  gvatrack tracking-type=${TRACKING_TYPE} ! \
  queue ! \
  gvaclassify model=$PERSON_CLASSIFICATION_MODEL \
              model-proc=$PERSON_CLASSIFICATION_MODEL_PROC \
              reclassify-interval=${RECLASSIFY_INTERVAL} \
              device=${INFERENCE_DEVICE} object-class=person ! \
  queue ! \
  gvaclassify model=$VEHICLE_CLASSIFICATION_MODEL \
              model-proc=$VEHICLE_CLASSIFICATION_MODEL_PROC \
              reclassify-interval=${RECLASSIFY_INTERVAL} \
              device=${INFERENCE_DEVICE} object-class=vehicle ! \
  queue ! \
  $SINK_ELEMENT"

echo ${PIPELINE}
${PIPELINE}
