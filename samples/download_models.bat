@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation

@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

setlocal

if NOT DEFINED INTEL_OPENVINO_DIR (
    echo ERROR: INTEL_OPENVINO_DIR not set, Install OpenVINO™ Toolkit and set OpenVINO™ Toolkit environment variables
    EXIT /B 1
)

set DOWNLOADER="%INTEL_OPENVINO_DIR%\deployment_tools\open_model_zoo\tools\downloader\downloader.py"
set CONVERTER="%INTEL_OPENVINO_DIR%\deployment_tools\open_model_zoo\tools\downloader\converter.py"

if NOT DEFINED MODELS_PATH (
    echo ERROR: MODELS_PATH not set
    EXIT /B 1
)

echo Downloading models to folder %MODELS_PATH%

mkdir "%MODELS_PATH%"

python %DOWNLOADER% --list "%~dp0\models.lst" -o "%MODELS_PATH%"
python %CONVERTER% --list "%~dp0\models.lst" -o "%MODELS_PATH%" -d "%MODELS_PATH%"
