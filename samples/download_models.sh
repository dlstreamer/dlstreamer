#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

DOWNLOADER=$INTEL_OPENVINO_DIR/deployment_tools/open_model_zoo/tools/downloader/downloader.py

echo Downloading models to folder "$MODELS_PATH"

mkdir -p ${MODELS_PATH} && python3 $DOWNLOADER --list $(dirname "$0")/models.lst -o $MODELS_PATH
