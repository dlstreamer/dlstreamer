#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Show help message
show_help() {
    cat <<EOF

Usage: $(basename "$0") [OPTIONS]

This script provides a simple way to construct and execute
a sample DL Streamer detection pipeline with YOLO model.

Options:
  -h, --help                  Show this help message and exit
  -m=MODEL, --model=MODEL     YOLO model name to be used for detection (default: yolo11s)
                              Available models: yolov5s | yolov8s | yolo11s
  -o=OUTPUT, --output=OUTPUT  Type of output from a pipeline (default: display)
                              Available types: display | file
  -d=DEVICE, --device=DEVICE  Device to perform detection operations on (default: CPU)
                              Available devices: CPU | GPU | NPU
                              Note: GPU and NPU devices require drivers (DLS_install_prerequisites.sh)

Examples:
  $(basename "$0")
  $(basename "$0") --model=yolov8s
  $(basename "$0") --model=yolov8s --output=file --device=NPU
  $(basename "$0") --help

EOF
}

# Parse options
MODEL="yolo11s"
OUTPUT="display"
DEVICE="CPU"
for arg in "$@"; do
    case $arg in
        -h|--help)
            show_help
            exit 0
            ;;
        -m=*|--model=*)
            MODEL="${arg#*=}"
            if [[ "$MODEL" != "yolov5s" ]] && [[ "$MODEL" != "yolov8s" ]] && [[ "$MODEL" != "yolo11s" ]]; then
                echo "Error! Wrong MODEL parameter. Supported models: yolov5s | yolov8s | yolo11s"
                exit 1
            fi
        ;;
        -o=*|--output=*)
            OUTPUT="${arg#*=}"
            if [[ "$OUTPUT" != "display" ]] && [[ "$OUTPUT" != "file" ]]; then
                echo "Error! Wrong value for OUTPUT parameter. Supported values: display | file"
                exit 1
            fi
        ;;
        -d=*|--device=*)
            DEVICE="${arg#*=}"
            if [[ "$DEVICE" != "CPU" ]] && [[ "$DEVICE" != "GPU" ]] && [[ "$DEVICE" != "NPU" ]]; then
                echo "Error! Wrong value for DEVICE parameter. Supported values: CPU | GPU | NPU"
                exit 1
            fi
        ;;
        *)
            echo "Unknown option: '$arg'"
            show_help
            exit 1
        ;;
    esac
done

# shellcheck source=/dev/null
. /etc/os-release

# variables required by DL Streamer
export LIBVA_DRIVER_NAME=iHD
export GST_PLUGIN_PATH=/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/opt/intel/dlstreamer/gstreamer/lib/
export LD_LIBRARY_PATH=/opt/intel/dlstreamer/gstreamer/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/lib/gstreamer-1.0:/usr/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/opencv:/opt/openh264:/opt/rdkafka:/opt/ffmpeg:/usr/local/lib/gstreamer-1.0:/usr/local/lib
export GST_VA_ALL_DRIVERS=1
export PYTHONPATH=/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:/opt/intel/dlstreamer/python:/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages
export PATH=/opt/intel/dlstreamer/gstreamer/bin:/opt/intel/dlstreamer/build/intel64/Release/bin:$HOME/.local/bin:$HOME/python3venv/bin:$PATH
if [ "$ID" == "ubuntu" ]; then
    export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
    export GI_TYPELIB_PATH=/opt/intel/dlstreamer/gstreamer/lib/girepository-1.0:/usr/lib/x86_64-linux-gnu/girepository-1.0
    DLS_VERSION=$(dpkg -s intel-dlstreamer | grep '^Version:' | sed -En "s/Version: (.*)/\1/p")
elif [ "$ID" == "fedora" ] || [ "$ID" == "rhel" ]; then
    export LIBVA_DRIVERS_PATH=/usr/lib64/dri-nonfree
    export GI_TYPELIB_PATH=/opt/intel/dlstreamer/gstreamer/lib/girepository-1.0:/usr/lib64/girepository-1.0
    DLS_VERSION=$(rpm -q --qf '%{VERSION}\n' intel-dlstreamer)
else
    echo "Unsupported system: $ID $VERSION_ID"
    exit 1
fi

# variables required by test pipeline
export MODELS_PATH="$HOME"/models

# remove gstreamer cache
rm -rf ~/.cache/gstreamer-1.0

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

# check if the model exists
if [ -d "$MODELS_PATH"/public/"$MODEL"/FP32 ]; then
	echo "$MODEL model exists."
else
	echo "Model $MODEL which you want to use cannot be found!"
	echo "Please run the script \`/opt/intel/dlstreamer/samples/download_public_models.sh $MODEL\` to download the model."
	echo "If the model has already been downloaded, specify the path to its location."
	exit 1
fi

echo ""
echo "Testing sample pipeline:"
echo ""

export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

# print pipeline and run it
execute() { echo "$*"$'\n' ; "$@" ; }
execute /opt/intel/dlstreamer/samples/gstreamer/gst_launch/detection_with_yolo/yolo_detect.sh $MODEL "$DEVICE" '' "$OUTPUT"
