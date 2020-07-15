#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

AUDIO_MODELS_PATH=$(dirname "$0")/models

echo Downloading model to "$AUDIO_MODELS_PATH" directory
AUIDO_ONNX_MODEL_SOURCE="https://download.01.org/opencv/models_contrib/sound_classification/aclnet/pytorch/15062020/aclnet_des_53_fp32.onnx"

AUIDO_MODEL_NAME=aclnet_des_53_fp32

AUIDO_MODEL_ONNX_DESTINATION=${AUDIO_MODELS_PATH}/$AUIDO_MODEL_NAME.onnx
mkdir -p ${AUDIO_MODELS_PATH} 
wget --tries=1 --timeout=90 $AUIDO_ONNX_MODEL_SOURCE --output-document=$AUIDO_MODEL_ONNX_DESTINATION


if [ -z "$INTEL_OPENVINO_DIR" ]
then
    echo -e "\e[31mERROR: INTEL_OPENVINO_DIR not set, Install OpenVINO and set OpenVINO environment variables\e[0m" 1>&2
    exit 1
fi

MO_PATH=$INTEL_OPENVINO_DIR/deployment_tools/model_optimizer

if [ ! -d "$MO_PATH" ]
then
    echo -e "\e[31mERROR: invalid model_optimizer directory $MO_PATH, install OpenVINO model optimizer\e[0m" 1>&2
    exit 1
fi

MO=$MO_PATH/mo.py

echo -e "\e[31mInstalling pre requisites for onnx to IR conversion, may need password to run as sudo \e[0m" 1>&2
INSTALL_PREREQ=$MO_PATH/install_prerequisites/install_prerequisites.sh 
( "$INSTALL_PREREQ" )

echo Converting onnx to IR model, saving in "$AUDIO_MODELS_PATH" directory
python3  $MO --framework onnx --batch 1 --input_model $AUIDO_MODEL_ONNX_DESTINATION --data_type FP32 --output_dir $AUDIO_MODELS_PATH  --model_name $AUIDO_MODEL_NAME