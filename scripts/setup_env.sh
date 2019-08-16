#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

export GST_VAAPI_ALL_DRIVERS=1

export GST_SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
export GST_HOME=${GST_SCRIPT_DIR}/..
export GST_SAMPLES_DIR=${GST_HOME}/samples

DEFAULT_VIDEO_DIR=${GST_HOME}/../video-examples
GST_PLUGIN_BUILD=${GST_HOME}/build/intel64/Release/lib

export VIDEO_EXAMPLES_DIR=${VIDEO_EXAMPLES_DIR:-$DEFAULT_VIDEO_DIR}
export GST_PLUGIN_PATH=$GST_PLUGIN_BUILD:$GST_PLUGIN_PATH

export MODELS_PATH=${MODELS_PATH:-/opt/intel/openvino/deployment_tools/intel_models}

echo [setup_env.sh] GStreamer-plugins environment initialized
