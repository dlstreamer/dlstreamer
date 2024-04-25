#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -euo pipefail

MODEL=${1:-"yolo_all"} # Supported values: yolo_all, yolox-tiny, yolox_s, yolov7, yolov8s, yolov9c

SUPPORTED_MODELS=(
  "yolo_all"
  "yolox-tiny"
  "yolox_s"
  "yolov5s"
  "yolov5su"
  "yolov7"
  "yolov8s"
  "yolov9c"
)

if ! [[ "${SUPPORTED_MODELS[*]}" =~ $MODEL ]]; then
  echo "Unsupported model: $MODEL" >&2
  exit 1
fi

# check if model exists in local directory, download as needed
if [[ "$MODEL" == "yolox-tiny" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolox-tiny"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    omz_downloader --name "$MODEL_NAME"
    omz_converter --name "$MODEL_NAME"
  fi
fi

if [[ "$MODEL" == "yolox_s" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolox_s"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Model not found: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    wget https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s_openvino.tar.gz
    tar -xvf ./yolox_s_openvino.tar.gz
    rm ./yolox_s_openvino.tar.gz
    cd -
  fi
fi

if [[ "$MODEL" == "yolov5su" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolov5su"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p ./public/"$MODEL_NAME"
    cd ./public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov5s.pt")
model.info()
model.export(format='openvino')  # creates 'yolov5su_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP32
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov5s.pt")
model.info()
model.export(format='openvino', half=True)  # creates 'yolov5su_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP16
    rm -rf yolov5su_openvino_model
    cd ../..
  fi
fi

if [[ "$MODEL" == "yolov5s" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolov5s"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$PWD
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/ultralytics/yolov5
    cd yolov5
    wget https://github.com/ultralytics/yolov5/releases/download/v7.0/yolov5s.pt
    python3 export.py --weights yolov5s.pt --include openvino
    mv yolov5s_openvino_model/yolov5s.xml "$MODEL_DIR"/yolov5s.xml
    mv yolov5s_openvino_model/yolov5s.bin "$MODEL_DIR"/yolov5s.bin
    cd ..
    rm -rf yolov5
    cd "$PREV_DIR"
  fi
fi

if [[ "$MODEL" == "yolov7" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolov7"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  MODEL_DIR=$(dirname "$MODEL_PATH")
  PREV_DIR=$PWD
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p "$MODEL_DIR"
    cd "$MODEL_DIR"
    git clone https://github.com/WongKinYiu/yolov7.git
    cd yolov7
    python3 export.py --weights yolov7.pt --grid
    ovc yolov7.onnx
    mv yolov7.xml "$MODEL_PATH"
    mv yolov7.bin "$MODEL_DIR"
    cd ..
    rm -rf yolov7
    cd "$PREV_DIR"
  fi
fi

if [[ "$MODEL" == "yolov8s" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolov8s"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p ./public/"$MODEL_NAME"
    cd ./public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov8s.pt")
model.info()
model.export(format='openvino')  # creates 'yolov8s_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP32
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov8s.pt")
model.info()
model.export(format='openvino', half=True)  # creates 'yolov8s_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP16
    cd ../..
  fi
fi

if [[ "$MODEL" == "yolov9c" ]] || [[ "$MODEL" == "yolo_all" ]]; then
  MODEL_NAME="yolov9c"
  MODEL_PATH="$PWD/public/$MODEL_NAME/FP32/$MODEL_NAME.xml"
  if [ ! -f "$MODEL_PATH" ]; then
    echo "Downloading and converting: ${MODEL_PATH}"
    mkdir -p ./public/"$MODEL_NAME"
    cd ./public/"$MODEL_NAME"
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov9c.pt")
model.info()
model.export(format='openvino')  # creates 'yolov9c_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP32
    python3 - <<EOF
from ultralytics import YOLO
model = YOLO("yolov9c.pt")
model.info()
model.export(format='openvino', half=True)  # creates 'yolov9c_openvino_model/'
EOF
    mv ./"${MODEL_NAME}"_openvino_model ./FP16
    cd ../..
  fi
fi
