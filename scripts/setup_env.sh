#!/bin/bash
# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

export LIBVA_DRIVER_NAME=iHD
export GST_VAAPI_ALL_DRIVERS=1
export LIBVA_DRIVERS_PATH=/opt/intel/mediasdk/lib64

export GST_SAMPLES_DIR=$(dirname "$BASH_SOURCE")/../samples

DEFAULT_VIDEO_DIR=${GST_SAMPLES_DIR}/../../video-examples
DEFAULT_GST_PLUGIN_PATH=${GST_SAMPLES_DIR}/../build/intel64/Release/lib

export VIDEO_EXAMPLES_DIR=${VIDEO_EXAMPLES_DIR:-$DEFAULT_VIDEO_DIR}
export GST_PLUGIN_PATH=${GST_PLUGIN_PATH:-$DEFAULT_GST_PLUGIN_PATH}

export MODELS_PATH=${MODELS_PATH:-/opt/intel/computer_vision_sdk/deployment_tools/intel_models}

echo [setup_env.sh] GStreamer-plugins environment initialized
