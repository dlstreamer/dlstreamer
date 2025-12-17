#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
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

# check if the inference model is set correctly; yolov5s, yolov8s and yolo11s are supported on this level
MODEL=${3:-"yolo11s"}
if [[ "$MODEL" != "yolov5s" ]] && [[ "$MODEL" != "yolov8s" ]] && [[ "$MODEL" != "yolo11s" ]]; then
    echo "Error! Wrong MODEL parameter. Supported models: yolov5s | yolov8s | yolo11s".
    exit
fi
if [ -d "$MODELS_PATH"/public/"$MODEL"/FP32 ]; then
	echo "$MODEL model exists."
else
	echo "Model $MODEL which you want to use cannot be found!"
	echo "Please run the script `/opt/intel/dlstreamer/samples/download_public_models.sh $MODEL` to download the model."
	echo "If the model has already been downloaded, specify the path to its location."
	exit 1
fi

echo ""
echo "Testing sample pipeline:"
echo ""

export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

# check if the output is set to file or display, only two options are supported
OUTPUT=${1:-"display"}
if [[ "$OUTPUT" != "display" ]] && [[ "$OUTPUT" != "file" ]]; then
    echo "Error! Wrong value for OUTPUT parameter. Supported values: display | file".
    exit
fi
# check if the device for inference is set correctly; only CPU, GPU and NPU options are supported
DEVICE=${2:-"CPU"}
if [[ "$DEVICE" != "CPU" ]] && [[ "$DEVICE" != "GPU" ]] && [[ "$DEVICE" != "NPU" ]]; then
    echo "Error! Wrong value for DEVICE parameter. Supported values: CPU | GPU | NPU".
    exit
fi

# print pipeline and run it
execute() { echo "$*"$'\n' ; "$@" ; }
execute /opt/intel/dlstreamer/samples/gstreamer/gst_launch/detection_with_yolo/yolo_detect.sh $MODEL "$DEVICE" '' "$OUTPUT"