#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

REPO_PATH=`realpath $SCRIPTPATH/../../..`

mkdir -p $SCRIPTPATH/src

cp -r $REPO_PATH/gst-libs/gst/videoanalytics $REPO_PATH/python/gstgva $SCRIPTPATH/src
