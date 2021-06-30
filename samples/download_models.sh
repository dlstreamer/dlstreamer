#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

DOWNLOADER=$INTEL_OPENVINO_DIR/deployment_tools/open_model_zoo/tools/downloader/downloader.py
CONVERTER=$INTEL_OPENVINO_DIR/deployment_tools/open_model_zoo/tools/downloader/converter.py

echo Downloading models to folder "$MODELS_PATH"

mkdir -p ${MODELS_PATH} && \
python3 $DOWNLOADER --list $(dirname "$0")/models.lst -o $MODELS_PATH && \
python3 $CONVERTER --list $(dirname "$0")/models.lst -o $MODELS_PATH -d $MODELS_PATH
