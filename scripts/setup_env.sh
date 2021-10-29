#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

GST_SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]:-${(%):-%x}}" )" >/dev/null 2>&1 && pwd )"
export GST_PLUGIN_PATH=$( realpath ${GST_SCRIPT_DIR}/../build/intel64 ):${GST_PLUGIN_PATH}
export PYTHONPATH=$( realpath ${GST_SCRIPT_DIR}/../python ):${PYTHONPATH}

export MODELS_PATH=${MODELS_PATH:-${HOME}/intel/dl_streamer/models}

export LC_NUMERIC="C"

echo "[setup_env.sh] GVA-plugins environment initialized"
