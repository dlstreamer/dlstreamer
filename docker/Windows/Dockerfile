# escape=`
# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG BASE_IMAGE=mcr.microsoft.com/windows/servercore/iis
FROM ${BASE_IMAGE}

SHELL ["cmd", "/S", "/C"]

RUN mkdir C:\temp

# Install VCRedist
RUN powershell.exe -Command Invoke-WebRequest -URI https://aka.ms/vs/16/release/vc_redist.x64.exe -OutFile c:/temp/vc_redist.x64.exe
RUN powershell.exe -Command Start-Process c:/temp/vc_redist.x64.exe -ArgumentList '/quiet /norestart' -Wait ; `
    Remove-Item "c:/temp/vc_redist.x64.exe" -Force

# Install VS Build Tools
RUN powershell.exe -Command Invoke-WebRequest -URI https://aka.ms/vs/16/release/channel `
    -OutFile C:\temp\VisualStudio.chman
RUN powershell.exe -Command Invoke-WebRequest -URI https://aka.ms/vs/16/release/vs_buildtools.exe `
    -OutFile C:\temp\vs_buildtools.exe
RUN C:\temp\vs_buildtools.exe --quiet --wait --norestart `
    --installPath C:\BuildTools `
    --channelUri C:\temp\VisualStudio.chman `
    --installChannelUri C:\temp\VisualStudio.chman `
    --add Microsoft.VisualStudio.Workload.MSBuildTools `
    --add Microsoft.VisualStudio.Workload.VCTools `
    --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    --add Microsoft.VisualStudio.Component.Windows10SDK.18362 `
    --add Microsoft.VisualStudio.Component.VC.CMake.Project `
    --add Microsoft.VisualStudio.Component.TestTools.BuildTools `
    --add Microsoft.VisualStudio.Component.VC.ASAN

#Install Python
ARG PYTHON_VER=python3.7
RUN powershell.exe -Command Invoke-WebRequest -URI https://www.python.org/ftp/python/3.7.9/python-3.7.9-amd64.exe `
    -OutFile C:/temp/python-3.7.9-amd64.exe
RUN powershell.exe -Command Start-Process C:/temp/python-3.7.9-amd64.exe `
    -ArgumentList '/quiet InstallAllUsers=1 PrependPath=1 TargetDir=c:\\Python37' -Wait ; `
    Remove-Item  C:/temp/python-3.7.9-amd64.exe -Force
RUN python -m pip install --upgrade pip

# Install OpenVINO
ARG OPENVINO_DOWNLOAD_URL
ARG OPENVINO_VER

RUN powershell.exe -Command Invoke-WebRequest `
    -URI $env:OPENVINO_DOWNLOAD_URL `
    -OutFile C:/temp/w_openvino_toolkit_p_$env:OPENVINO_VER.exe
RUN powershell.exe -Command Start-Process C:/temp/w_openvino_toolkit_p_$env:OPENVINO_VER.exe `
    -ArgumentList '--s --a install --eula=accept --installdir=C:/intel --output=%TMP%/openvino_install_out.log --components=OPENVINO_COMMON,INFERENCE_ENGINE,INFERENCE_ENGINE_SDK,INFERENCE_ENGINE_SAMPLES,OMZ_TOOLS,POT,INFERENCE_ENGINE_CPU,INFERENCE_ENGINE_GPU,MODEL_OPTIMIZER,OMZ_DEV,OPENCV_PYTHON,OPENCV_RUNTIME,OPENCV,DOCS,SETUPVARS,VC_REDIST_2017_X64,icl_redist' `
    -Wait
RUN setx INTEL_OPENVINO_DIR "C:\intel\openvino_%OPENVINO_VER%"

# Install dependencies
RUN setx path "%PATH%;%APPDATA%\Python\Python37\Scripts"
RUN cd %INTEL_OPENVINO_DIR%\deployment_tools\model_optimizer\install_prerequisites && install_prerequisites.bat
RUN python -m pip install -r %INTEL_OPENVINO_DIR%\deployment_tools\open_model_zoo\tools\downloader\requirements.in

# Create symlink if neccessary
RUN powershell.exe -Command if ( -not (Test-Path -Path C:/intel/openvino_2021) ) `
    { New-Item -Path C:/intel/openvino -ItemType SymbolicLink -Value %INTEL_OPENVINO_DIR% }

# Install GStreamer
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://gstreamer.freedesktop.org/data/pkg/windows/1.16.2/gstreamer-1.0-devel-msvc-x86_64-1.16.2.msi `
    -OutFile c:/temp/gstreamer-1.0-devel-msvc-x86_64-1.16.2.msi
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://gstreamer.freedesktop.org/data/pkg/windows/1.16.2/gstreamer-1.0-msvc-x86_64-1.16.2.msi `
    -OutFile c:/temp/gstreamer-1.0-msvc-x86_64-1.16.2.msi
RUN powershell.exe -Command `
    Start-Process c:/temp/gstreamer-1.0-devel-msvc-x86_64-1.16.2.msi -ArgumentList '/quiet' -Wait ; `
    Start-Process c:/temp/gstreamer-1.0-msvc-x86_64-1.16.2.msi -ArgumentList '/quiet' -Wait ; `
    Remove-Item c:/temp/*.msi -Force

RUN setx GST_PLUGIN_PATH "%GSTREAMER_1_0_ROOT_X86_64%\lib\gstreamer-1.0"
RUN setx path "%PATH%;%GSTREAMER_1_0_ROOT_X86_64%\bin"
RUN setx PKG_CONFIG_PATH "%PKG_CONFIG_PATH%;%GSTREAMER_1_0_ROOT_X86_64%\lib\pkgconfig"

# Install Pkg-config
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://download.gnome.org/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip `
    -OutFile C:/temp/pkg-config_0.26-1_win32.zip
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://download.gnome.org/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip `
    -OutFile C:/temp/gettext-runtime_0.18.1.1-2_win32.zip
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://ftp.acc.umu.se/pub/GNOME/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip `
    -OutFile C:/temp/glib_2.28.8-1_win32.zip
RUN powershell.exe -Command `
    Expand-Archive C:/temp/pkg-config_0.26-1_win32.zip -DestinationPath C:/pkg-config -Force ; `
    Expand-Archive C:/temp/gettext-runtime_0.18.1.1-2_win32.zip -DestinationPath C:/pkg-config -Force ; `
    Expand-Archive C:/temp/glib_2.28.8-1_win32.zip -DestinationPath C:/pkg-config -Force ; `
    Remove-Item C:/temp/*.zip -Force
RUN setx path "%PATH%;c:/pkg-config/bin"

# Install CMake
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://github.com/Kitware/CMake/releases/download/v3.19.4/cmake-3.19.4-win64-x64.msi `
    -OutFile c:/temp/cmake-3.19.4-win64-x64.msi
RUN powershell.exe -Command `
    Start-Process c:/temp/cmake-3.19.4-win64-x64.msi -ArgumentList '/quiet /norestart' -Wait ; `
    Remove-Item c:/temp/cmake-3.19.4-win64-x64.msi -Force
RUN setx path "%PATH%;c:/Program Files/CMake/bin"

# Install Git
RUN powershell.exe -Command Invoke-WebRequest `
    -URI https://github.com/git-for-windows/git/releases/download/v2.30.1.windows.1/Git-2.30.1-64-bit.exe `
    -OutFile c:/temp/Git-2.30.1-64-bit.exe
RUN powershell.exe -Command `
    Start-Process c:/temp/Git-2.30.1-64-bit.exe -ArgumentList '/VERYSILENT /NORESTART' -Wait ; `
    Remove-Item c:/temp/Git-2.30.1-64-bit.exe -Force

# Install DL Streamer
ARG GST_GIT_URL="https://github.com/openvinotoolkit/dlstreamer_gst.git"
ARG GST_GIT_BRANCH="preview/support-for-windows"

RUN git clone -b %GST_GIT_BRANCH% %GST_GIT_URL% C:/dl-streamer && cd C:/dl-streamer && git status
RUN mkdir "C:\dl-streamer\build"
RUN %INTEL_OPENVINO_DIR%/bin/setupvars.bat `
    && cd "C:\dl-streamer\build" `
    && cmake .. `
    && cmake --build . --config Release

WORKDIR C:/dl-streamer/win_samples

ENTRYPOINT ["C:\\BuildTools\\Common7\\Tools\\VsDevCmd.bat", "&&", `
            "%INTEL_OPENVINO_DIR%/bin/setupvars.bat", "&&", `
            "C:/dl-streamer/win_scripts/setup_env.bat", "&&", `
            "powershell.exe", "-NoLogo", "-ExecutionPolicy", "Bypass"]