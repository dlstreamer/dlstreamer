#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -euo pipefail

# Default values
DEFAULT_SOURCE="https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4"
DEFAULT_DEVICE="CPU"
DEFAULT_PROMPT="Describe what you see in this video."
DEFAULT_FRAME_RATE="1"
DEFAULT_CHUNK_SIZE="10"
DEFAULT_MAX_NEW_TOKENS="100"
DEFAULT_METRICS="false"

# Function to display usage information
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Video Summarization with MiniCPM-V model using gvagenai element"
    echo ""
    echo "Options:"
    echo "  -s, --source FILE/URL/CAMERA      Input source (file path, URL or web camera)"
    echo "  -d, --device DEVICE               Inference device (CPU, GPU, NPU)"
    echo "  -p, --prompt TEXT                 Text prompt for the model"
    echo "  -r, --frame-rate RATE             Frame sampling rate (fps)"
    echo "  -c, --chunk-size NUM              Chunk size, or frames per inference call"
    echo "  -t, --max-tokens NUM              Maximum new tokens to generate"
    echo "  -m, --metrics                     Include performance metrics in JSON output"
    echo "  -h, --help                        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --source video.mp4 --device GPU"
    echo "  $0 --chunk-size 1 --frame-rate 10"
    echo "  $0 --prompt \"Describe what do you see in this video?\""
    echo "  $0 --metrics --max-tokens 200"
    echo ""
}

# Check if MINICPM_MODEL_PATH is set
if [ -z "${MINICPM_MODEL_PATH:-}" ]; then
    echo "ERROR - MINICPM_MODEL_PATH environment variable is not set." >&2
    echo "Please set it to the path where your MiniCPM-V model is located." >&2
    echo "Example: export MINICPM_MODEL_PATH=/path/to/minicpm-v-model" >&2
    exit 1
fi

# Initialize variables with defaults
INPUT="$DEFAULT_SOURCE"
DEVICE="$DEFAULT_DEVICE"
PROMPT="$DEFAULT_PROMPT"
FRAME_RATE="$DEFAULT_FRAME_RATE"
CHUNK_SIZE="$DEFAULT_CHUNK_SIZE"
MAX_NEW_TOKENS="$DEFAULT_MAX_NEW_TOKENS"
METRICS="$DEFAULT_METRICS"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--source)
            INPUT="$2"
            shift 2
            ;;
        -d|--device)
            DEVICE="$2"
            shift 2
            ;;
        -p|--prompt)
            PROMPT="$2"
            shift 2
            ;;
        -r|--frame-rate)
            FRAME_RATE="$2"
            shift 2
            ;;
        -f|--chunk-size)
            CHUNK_SIZE="$2"
            shift 2
            ;;
        -t|--max-tokens)
            MAX_NEW_TOKENS="$2"
            shift 2
            ;;
        -m|--metrics)
            METRICS="true"
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            # Support legacy positional arguments for backwards compatibility
            if [ $# -ge 1 ] && [ -z "${INPUT_SET:-}" ]; then
                INPUT="$1"
                INPUT_SET=1
            elif [ $# -ge 1 ] && [ -z "${DEVICE_SET:-}" ]; then
                DEVICE="$1"
                DEVICE_SET=1
            elif [ $# -ge 1 ] && [ -z "${PROMPT_SET:-}" ]; then
                PROMPT="$1"
                PROMPT_SET=1
            else
                echo "Unknown option: $1" >&2
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate arguments
if [[ "$DEVICE" != "CPU" && "$DEVICE" != "GPU" && "$DEVICE" != "NPU" ]]; then
    echo "ERROR - Invalid device: $DEVICE. Use CPU, GPU, or NPU." >&2
    exit 1
fi

# Print configuration
echo "=== MiniCPM-V with gvagenai Configuration ==="
echo "Model Path: $MINICPM_MODEL_PATH"
echo "Source: $INPUT"
echo "Device: $DEVICE"
echo "Prompt: $PROMPT"
echo "Frame Rate: $FRAME_RATE fps"
echo "Chunk Size: $CHUNK_SIZE"
echo "Max New Tokens: $MAX_NEW_TOKENS"
echo "Metrics: $METRICS"
echo "==========================================="

# Check if model exists
if [ ! -d "$MINICPM_MODEL_PATH" ]; then
    echo "ERROR - Model directory not found: $MINICPM_MODEL_PATH" >&2
    exit 1
fi

# Determine the source element based on the input
if [[ $INPUT == "/dev/video"* ]]; then
    SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ "$INPUT" == *"://"* ]]; then
    SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
    SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

# Generation configuration
GENERATION_CONFIG="max_new_tokens=${MAX_NEW_TOKENS}"

# Create the pipeline
OUTPUT_FILE="minicpm_output.json"

PIPELINE="gst-launch-1.0 \
    $SOURCE_ELEMENT ! \
    decodebin3 ! \
    videoconvert ! \
    video/x-raw,format=RGB ! \
    gvagenai \
        device=$DEVICE \
        model-path=\"$MINICPM_MODEL_PATH\" \
        prompt=\"$PROMPT\" \
        generation-config=\"$GENERATION_CONFIG\" \
        frame-rate=$FRAME_RATE \
        chunk-size=$CHUNK_SIZE \
        metrics=$METRICS ! \
    gvametapublish file-path=$OUTPUT_FILE ! \
    fakesink async=false"

echo ""
echo "Running MiniCPM-V inference pipeline..."
echo "Pipeline: $PIPELINE"
echo ""

eval "$PIPELINE"

echo ""
echo "Pipeline execution completed."
echo "Results saved to: $OUTPUT_FILE"
