@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation

@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

set GST_PLUGIN_PATH=%~dp0\..\build\intel64\Release\bin\Release;%GST_PLUGIN_PATH%
set PATH=%~dp0\..\build\intel64\Release\bin\Release;%PATH%
set PYTHONPATH=%~dp0\..\python;%PYTHONPATH%

if NOT DEFINED MODELS_PATH (
    set MODELS_PATH=%USERPROFILE%\intel\dl_streamer\models
)

set LC_NUMERIC="C"

echo [setup_env.sh] GVA-plugins environment initialized
