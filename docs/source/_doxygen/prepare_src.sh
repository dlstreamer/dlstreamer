#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

REPO_PATH=`realpath $SCRIPTPATH/../../..`

mkdir -p $SCRIPTPATH/src
mkdir -p $SCRIPTPATH/src-api2.0

cp -r $REPO_PATH/gst-libs/gst/videoanalytics $REPO_PATH/python/gstgva $REPO_PATH/include/dlstreamer/gst/metadata $SCRIPTPATH/src
cp -r $REPO_PATH/include/dlstreamer $SCRIPTPATH/src-api2.0

