@REM ==============================================================================
@REM Copyright (C) 2021-2022 Intel Corporation
@REM
@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off
setlocal

set help=Usage: %~nx0 OPENVINO_PACKAGE_URL IMAGE_TAG BASE_IMAGE

set openvino_package_url=%1
set image_tag=%2
set base_image=%3

if not defined openvino_package_url set openvino_package_url=https://storage.openvinotoolkit.org/repositories/openvino/packages/2022.1/w_openvino_toolkit_dev_p_2022.1.0.643.zip
if not defined image_tag set image_tag=latest
if not defined base_image set base_image=mcr.microsoft.com/windows/servercore/iis

set base_dir=%~dp0
set dockerfile=Dockerfile
set context_dir=%base_dir%

set openvino_version=%openvino_package_url:*dev_p_=%
set openvino_version=%openvino_version:.zip=%

docker build -f %base_dir%%dockerfile% -t dlstreamer:%image_tag% ^
     --build-arg OPENVINO_PACKAGE_URL="%openvino_package_url%" ^
     --build-arg OPENVINO_VERSION="%openvino_version%" ^
     --build-arg BASE_IMAGE="%base_image%" ^
     %context_dir%

exit /B %ERRORLEVEL%
