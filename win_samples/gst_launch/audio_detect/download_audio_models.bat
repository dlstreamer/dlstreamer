@REM ==============================================================================
@REM Copyright (C) 2021 Intel Corporation

@REM SPDX-License-Identifier: MIT
@REM ==============================================================================

@echo off

setlocal

if NOT DEFINED MODELS_PATH (
    echo [91mERROR: MODELS_PATH not set[0m
    EXIT /B 1
)

set "AUDIO_MODELS_PATH=%MODELS_PATH%\audio_models\aclnet\FP32"

echo Downloading model to %AUDIO_MODELS_PATH% directory
set AUIDO_ONNX_MODEL_SOURCE=https://download.01.org/opencv/models_contrib/sound_classification/aclnet/pytorch/15062020/aclnet_des_53_fp32.onnx

set AUIDO_MODEL_NAME=aclnet

set "AUIDO_MODEL_ONNX_DESTINATION=%AUDIO_MODELS_PATH%\aclnet_des_53.onnx"
mkdir %AUDIO_MODELS_PATH%

powershell -command "Invoke-WebRequest %AUIDO_ONNX_MODEL_SOURCE% -OutFile %AUIDO_MODEL_ONNX_DESTINATION% -TimeoutSec 90"

if NOT DEFINED MO_DIR (
    echo MO_DIR not set, Cheking for INTEL_OPENVINO_DIR
    if NOT DEFINED INTEL_OPENVINO_DIR (
        echo [91mERROR: INTEL_OPENVINO_DIR not set, Install OpenVINO™ Toolkit and set OpenVINO™ Toolkit environment variables[0m
        EXIT /B 1
    )

    set "MO_DIR=%INTEL_OPENVINO_DIR%\deployment_tools\model_optimizer"
    if NOT EXIST %MO_DIR%\ (
        echo [91mERROR: Invalid model_optimizer directory $MO_DIR, set model optimizer directory MO_DIR[0m
        EXIT /B 1
    )
)

set "MO=%MO_DIR%\mo.py"

echo Assuming you have installed pre requisites for onnx to IR conversion, if issues run %MO_DIR%\install_prerequisites\install_prerequisites.bat

echo Converting onnx to IR model, saving in %AUDIO_MODELS_PATH% directory
python "%MO%" --framework onnx --batch 1 --input_model "%AUIDO_MODEL_ONNX_DESTINATION%" --data_type FP32 --output_dir "%AUDIO_MODELS_PATH%"  --model_name %AUIDO_MODEL_NAME%

EXIT /B %ERRORLEVEL%
