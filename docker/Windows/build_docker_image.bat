@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation
@REM
@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off
setlocal

set help=Usage: %~nx0 OPENVINO_DOWNLOAD_URL IMAGE_TAG BASE_IMAGE

set openvino_url=%1
set image_tag=%2
set base_image=%3

if not defined openvino_url (
    echo %help%
    echo.
    echo Please provide url to OpenVINO™ Toolkit installer
    exit /B 1
)

if not defined image_tag set image_tag=latest
if not defined base_image set base_image=mcr.microsoft.com/windows/servercore/iis

set openvino_ver=%~n1
set openvino_ver=%openvino_ver:w_openvino_toolkit_p_=%
set openvino_ver=%openvino_ver:_online=%

set base_dir=%~dp0
set dockerfile=Dockerfile
set context_dir=%base_dir%

docker build -f %base_dir%%dockerfile% -t dl_streamer:%image_tag% ^
     --build-arg OPENVINO_DOWNLOAD_URL="%openvino_url%" ^
     --build-arg OPENVINO_VER="%openvino_ver%" ^
     --build-arg BASE_IMAGE="%base_image%" ^
     %context_dir%

exit /B %ERRORLEVEL%
