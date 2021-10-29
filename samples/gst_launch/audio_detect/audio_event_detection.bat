@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation

@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

setlocal

set INPUT=%1
if [%INPUT%]==[] set INPUT=%~dp0how_are_you_doing.wav

if NOT DEFINED AUDIO_MODELS_PATH (
    if NOT DEFINED MODELS_PATH (
        echo [91mERROR: Environment variables AUDIO_MODELS_PATH or MODELS_PATH not specified. Models not found, execute download_audio_models.bat to download models[0m
        EXIT /B 1
    ) else ( set "AUDIO_MODELS_PATH=%MODELS_PATH%" )
)

set MODEL_NAME=aclnet

setlocal EnableDelayedExpansion
if NOT x%INPUT:://=%==x%INPUT% (
    set SOURCE_ELEMENT=urisourcebin uri=%INPUT%
) else (
    set INPUT=%INPUT:\=/%
    set SOURCE_ELEMENT=filesrc location=!INPUT!
)

set "MODEL_PATH=%AUDIO_MODELS_PATH%\public\aclnet\FP32\%MODEL_NAME%.xml"
set "MODEL_PROC_PATH=%~dp0model_proc\%MODEL_NAME%.json"

if NOT EXIST "%MODEL_PROC_PATH%" (
    echo [91mERROR: Invalid model-proc file path %MODEL_PROC_PATH%[0m
    EXIT /B 1
)

@REM correcting paths as in Linux
set MODEL_PATH=%MODEL_PATH:\=/%
set MODEL_PROC_PATH=%MODEL_PROC_PATH:\=/%

setlocal DISABLEDELAYEDEXPANSION
set PIPELINE=gst-launch-1.0 %SOURCE_ELEMENT% ! ^
decodebin ! audioresample ! audioconvert ! audio/x-raw, channels=1,format=S16LE,rate=16000 ! audiomixer output-buffer-duration=100000000 ! ^
gvaaudiodetect model="%MODEL_PATH%" model-proc="%MODEL_PROC_PATH%" sliding-window=0.2 ^
! gvametaconvert ! gvametapublish file-format=json-lines ! fakesink
setlocal ENABLEDELAYEDEXPANSION

echo !PIPELINE!
!PIPELINE!

EXIT /B %ERRORLEVEL%
