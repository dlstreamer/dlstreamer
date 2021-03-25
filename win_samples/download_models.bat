@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation

@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

setlocal

if NOT DEFINED INTEL_OPENVINO_DIR (
    echo [91mERROR: INTEL_OPENVINO_DIR not set, Install OpenVINO™ Toolkit and set OpenVINO™ Toolkit environment variables[0m
    EXIT /B 1
)

set DOWNLOADER="%INTEL_OPENVINO_DIR%\deployment_tools\open_model_zoo\tools\downloader\downloader.py"

if NOT EXIST %DOWNLOADER% (
    echo [91mERROR: No model downloader found along %DOWNLOADER% path[0m
    EXIT /B 1
)

if NOT DEFINED MODELS_PATH (
    echo [91mERROR: MODELS_PATH not set[0m
    EXIT /B 1
)

echo Downloading models to folder %MODELS_PATH%

mkdir "%MODELS_PATH%"

python %DOWNLOADER% --list "%~dp0\models.lst" -o "%MODELS_PATH%"
