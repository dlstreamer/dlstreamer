@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation
@REM
@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

set BASE_DIR=%~dp0
set BUILD_DIR=%USERPROFILE%\intel\dl_streamer\samples\draw_face_attributes\build

if exist %BUILD_DIR%\ rmdir /S /Q "%BUILD_DIR%"
md %BUILD_DIR%
cd %BUILD_DIR%

cmake %BASE_DIR%
cmake --build . --config Release

cd %BASE_DIR%

set FILE=%1
if not defined FILE set FILE=https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4
%BUILD_DIR%/draw_face_attributes.exe -i %FILE%
