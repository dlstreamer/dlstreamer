#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z "$MODELS_PATH" ]; then
  echo "MODELS_PATH is not specified"
  echo "Please set MODELS_PATH env variable with target path to download models"
  exit 1
fi

echo Downloading models to folder "$MODELS_PATH"

if ! python3 -m pip show -qq openvino-dev; then
  echo "Script requires Open Model Zoo python modules, please install via 'python3 -m pip install openvino-dev[onnx]'";
  exit 1
fi

mkdir -p ${MODELS_PATH} && \
omz_downloader --list $(dirname "$0")/models.lst -o $MODELS_PATH && \
omz_converter --list $(dirname "$0")/models.lst -o $MODELS_PATH -d $MODELS_PATH
