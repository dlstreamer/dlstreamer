#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z "$MODELS_PATH" ]
then
  echo MODELS_PATH not set
  exit 1
fi

AUDIO_MODELS_PATH=$MODELS_PATH/audio_models/aclnet/FP32

echo Downloading model to "$AUDIO_MODELS_PATH" directory
AUIDO_ONNX_MODEL_SOURCE="https://download.01.org/opencv/models_contrib/sound_classification/aclnet/pytorch/15062020/aclnet_des_53_fp32.onnx"

AUIDO_MODEL_NAME=aclnet

AUIDO_MODEL_ONNX_DESTINATION=${AUDIO_MODELS_PATH}/"aclnet_des_53".onnx
mkdir -p ${AUDIO_MODELS_PATH} 
wget --tries=1 --timeout=90 $AUIDO_ONNX_MODEL_SOURCE --output-document=$AUIDO_MODEL_ONNX_DESTINATION

if [ ! -d "$MO_DIR" ]
then
    echo "MO_DIR not set, Cheking for INTEL_OPENVINO_DIR"
    if [ -z "$INTEL_OPENVINO_DIR" ]
    then
        echo -e "\e[31mERROR: INTEL_OPENVINO_DIR not set, Install OpenVINO™ Toolkit and set OpenVINO™ Toolkit environment variables\e[0m" 1>&2
        exit 1
    fi
    MO_DIR=$INTEL_OPENVINO_DIR/deployment_tools/model_optimizer
    if [ ! -d "$MO_DIR" ]
    then
        echo -e "\e[31mERROR: invalid model_optimizer directory $MO_DIR, set model optimizer directory MO_DIR\e[0m" 1>&2
        exit
    fi
fi

MO=$MO_DIR/mo.py

echo -e "\e[31m Assuming you have installed pre requisites for onnx to IR conversion, if issues run $MO_DIR/install_prerequisites/install_prerequisites.sh \e[0m" 1>&2

echo Converting onnx to IR model, saving in "$AUDIO_MODELS_PATH" directory
python3  $MO --framework onnx --batch 1 --input_model $AUIDO_MODEL_ONNX_DESTINATION --data_type FP32 --output_dir $AUDIO_MODELS_PATH  --model_name $AUIDO_MODEL_NAME