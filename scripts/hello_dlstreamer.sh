#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# variables required by Intel(R) DL Streamer
export LIBVA_DRIVER_NAME=iHD
export GST_PLUGIN_PATH=/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/opt/intel/dlstreamer/gstreamer/lib/:
export LD_LIBRARY_PATH=/opt/intel/dlstreamer/gstreamer/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/lib/gstreamer-1.0:/usr/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/opencv:/opt/openh264:/opt/rdkafka:/opt/ffmpeg:/usr/local/lib/gstreamer-1.0:/usr/local/lib
export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
export GST_VA_ALL_DRIVERS=1
export PYTHONPATH=/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:/home/dlstreamer/dlstreamer/python:/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:
export GI_TYPELIB_PATH=/opt/intel/dlstreamer/gstreamer/lib/girepository-1.0:/usr/lib/x86_64-linux-gnu/girepository-1.0
export PATH=/opt/intel/dlstreamer/gstreamer/bin:/opt/intel/dlstreamer/build/intel64/Release/bin:/home/$USER/.local/bin:/home/$USER/python3venv/bin:$PATH

# variables required by test pipeline
export MODELS_PATH=/home/"$USER"/models

# remove gstreamer cache
rm -rf ~/.cache/gstreamer-1.0

# get Intel(R) DL Streamer version
DLS_VERSION=$(dpkg -s intel-dlstreamer | grep '^Version:' | sed -En "s/Version: (.*)/\1/p")

if [ -z "$DLS_VERSION" ]; then
	echo "-------------------------------------"
	echo "Intel(R) DL Streamer is not installed"
	echo "-------------------------------------"
	# exit from script if instalation has failed
	exit 1
else
	echo "---------------------------------------------------------------------------------------"
	echo "Intel(R) DL Streamer ${DLS_VERSION} has been installed successfully. You are ready to use it."
	echo "---------------------------------------------------------------------------------------"
fi

if [ -d "$MODELS_PATH"/public/yolo11s/FP32 ]; then
	echo "Yolo11s model has already been downloaded"
else
	echo ""
	echo "Downloading model yolo11s:"
	echo "It may take up to 20mins, depending on network throughput..."
	echo ""

	mkdir -p "$MODELS_PATH"/public/yolo11s
	mkdir -p /home/"$USER"/python3venv
	python3 -m venv /home/"$USER"/python3venv
	/home/"$USER"/python3venv/bin/pip3 install --no-cache-dir --upgrade pip
	/home/"$USER"/python3venv/bin/pip3 install --no-cache-dir --no-dependencies PyGObject
	/home/"$USER"/python3venv/bin/pip3 install --no-cache-dir ultralytics openvino
 	# shellcheck source=/dev/null
 	source /home/"$USER"/python3venv/bin/activate
	/opt/intel/dlstreamer/samples/download_public_models.sh yolo11s
fi

echo ""
echo "Testing sample pipeline:"
echo ""

export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

# print pipeline and run it
execute() { echo "$*"$'\n' ; "$@" ; }
execute /opt/intel/dlstreamer/samples/gstreamer/gst_launch/detection_with_yolo/yolo_detect.sh yolo11s CPU '' display
