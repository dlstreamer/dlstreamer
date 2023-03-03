#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BUILD_TYPE=${1:-Release}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-${(%):-%x}}" )" >/dev/null 2>&1 && pwd )"
BUILD_DIR=$( realpath "$SCRIPT_DIR/../build" )

# If Release folder doesn't exist, use Debug
if [[ ! -d "${BUILD_DIR}/intel64/${BUILD_TYPE}" && -d "${BUILD_DIR}/intel64/Debug" ]]; then BUILD_TYPE=Debug; fi
LIB_PATH="${BUILD_DIR}/intel64/${BUILD_TYPE}/lib"

export GST_PLUGIN_PATH="$LIB_PATH":$( realpath ${BUILD_DIR}/../src/gst ):${GST_PLUGIN_PATH}

export PYTHONPATH=$( realpath ${BUILD_DIR}/../python ):${PYTHONPATH}

export MODELS_PATH=${MODELS_PATH:-${HOME}/intel/dl_streamer/models}

export LC_NUMERIC="C"

echo "Added path to GST_PLUGIN_PATH: $LIB_PATH"
