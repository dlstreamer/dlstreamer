#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

MODEL_TYPE="whisper"
DEVICE="CPU"
INPUT_SOURCE=""
PATH_TO_MODELS=""
TRANSCRIPTION_MODE=""



# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --input-source=*)
            INPUT_SOURCE="${1#*=}"
            shift
            ;;
        --models-path=*)
            PATH_TO_MODELS="${1#*=}"
            shift
            ;;
        --model-type=*)
            MODEL_TYPE="${1#*=}"
            shift
            ;;
        --device=*)
            DEVICE="${1#*=}"
            shift
            ;;
        --mode=*)
            TRANSCRIPTION_MODE="${1#*=}"
            shift
            ;;
        --input-source)
            INPUT_SOURCE="$2"
            shift 2
            ;;
        --models-path)
            PATH_TO_MODELS="$2"
            shift 2
            ;;
        --model-type)
            MODEL_TYPE="$2"
            shift 2
            ;;
        --device)
            DEVICE="$2"
            shift 2
            ;;
        --mode)
            TRANSCRIPTION_MODE="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --input-source=FILE    Path to input source (WAV file, video file)"
            echo "  --models-path=DIR      Path to models directory"
            echo "  --model-type=TYPE      Model type whisper"
            echo "  --device=DEVICE        Device to use (CPU, GPU, default: CPU)"
            echo "  --mode=MODE            Transcription mode (wav, live, video)"
            echo "  -h, --help             Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 --input-source=audio.wav --models-path=/path/to/models --device=CPU --mode=wav"
            echo "  $0 --input-source=video.mp4 --models-path=/path/to/models --device=CPU --mode=video"
            echo "  $0 --models-path=/path/to/models --mode=live"
            echo ""
            exit 0
            ;;
        *)
            # Handle positional arguments (legacy support)
            if [ -z "$INPUT_SOURCE" ]; then
                INPUT_SOURCE="$1"
            elif [ -z "$PATH_TO_MODELS" ]; then
                PATH_TO_MODELS="$1"
            elif [ "$MODEL_TYPE" = "whisper" ]; then
                MODEL_TYPE="$1"
            elif [ "$DEVICE" = "CPU" ]; then
                DEVICE="$1"
            elif [ -z "$TRANSCRIPTION_MODE" ]; then
                TRANSCRIPTION_MODE="$1"
            else
                echo "Unknown parameter: $1"
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate transcription mode
case "$TRANSCRIPTION_MODE" in
    "wav"|"live"|"video")
        echo "Transcription mode: $TRANSCRIPTION_MODE"
        ;;
    *)
        echo "Error: Invalid transcription mode '$TRANSCRIPTION_MODE'"
        echo "Valid modes are: wav, live, video"
        exit 1
        ;;
esac

# For live mode, input source is not required
if [ "$TRANSCRIPTION_MODE" != "live" ] && [ -z "$INPUT_SOURCE" ]; then
    echo "Error: --input-source is required for mode '$TRANSCRIPTION_MODE'"
    exit 1
fi

# Additional validation based on transcription mode
case "$TRANSCRIPTION_MODE" in
    "wav")
        echo "WAV file transcription mode"
        echo "Make sure your input source '$INPUT_SOURCE' is a wav file."
        ;;
    "video")
        echo "Video file audio transcription mode"
        echo "Make sure your input source '$INPUT_SOURCE' is a MP4 video file."
        ;;
    "live")
        echo "Live transcription mode: Using microphone input"
        echo "Note: Make sure a microphone is connected and accessible"
        ;;
esac

# Build pipeline based on transcription mode
case "$TRANSCRIPTION_MODE" in
    "wav")
        echo "Building WAV file transcription pipeline..."
        PIPELINE="filesrc location=$INPUT_SOURCE ! decodebin3 ! audioresample ! audioconvert ! audio/x-raw,channels=1,format=S16LE,rate=16000 ! audiomixer output-buffer-duration=100000000 ! gvaaudiotranscribe model=$PATH_TO_MODELS device=$DEVICE model_type=$MODEL_TYPE ! fakesink"
        ;;
    "video")
        echo "Building video file audio transcription pipeline..."
        PIPELINE="filesrc location=$INPUT_SOURCE ! qtdemux name=demux demux.audio_0 ! decodebin ! audioconvert ! audioresample ! audio/x-raw,channels=1,format=S16LE,rate=16000 ! audiomixer output-buffer-duration=100000000 ! gvaaudiotranscribe model=$PATH_TO_MODELS device=$DEVICE model_type=$MODEL_TYPE ! fakesink"
        ;;
    "live")
        echo "Building live microphone transcription pipeline..."
        PIPELINE="pulsesrc buffer-time=2000000 ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,channels=1,rate=16000 ! queue max-size-buffers=100 max-size-time=0 max-size-bytes=0 ! gvaaudiotranscribe model=$PATH_TO_MODELS device=$DEVICE model_type=$MODEL_TYPE ! fakesink"
        ;;
    *)
        echo "Error: Unknown transcription mode '$TRANSCRIPTION_MODE'"
        exit 1
        ;;
esac

echo ""
echo "Pipeline configuration:"
echo "  Pipeline: $PIPELINE"
echo ""
echo "Starting transcription..."
echo "Press Ctrl+C to stop"
echo ""

# Execute the GStreamer pipeline
echo "gst-launch-1.0 $PIPELINE"
gst-launch-1.0 $PIPELINE
exit_code=$?

# Check the exit status
if [ $exit_code -eq 0 ]; then
    echo ""
    echo "Transcription completed successfully!"
else
    echo ""
    echo "Transcription failed with exit code $exit_code"
    exit 1
fi


