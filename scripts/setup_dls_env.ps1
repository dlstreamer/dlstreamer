# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

if (-Not (Test-Path 'C:\gstreamer')) {
	echo 'Please install GStreamer to folder C:\gstreamer and then run the script again.'
	exit
} else {
	echo 'GStreamer found in folder C:\gstreamer'
}

echo 'Setting variables: LIBVA_DRIVER_NAME, LIBVA_DRIVERS_PATH, Path (for LIBVA)'
[Environment]::SetEnvironmentVariable('LIBVA_DRIVER_NAME', 'vaon12', [System.EnvironmentVariableTarget]::User)
[Environment]::SetEnvironmentVariable('LIBVA_DRIVERS_PATH', (Get-Item .).FullName + '\Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2\build\native\x64\bin\', [System.EnvironmentVariableTarget]::User)
$USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
if (-Not $USER_PATH.Contains('VideoAccelerationCompatibilityPack')) {
	[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ';' + [Environment]::GetEnvironmentVariable('LIBVA_DRIVERS_PATH', 'User'), [System.EnvironmentVariableTarget]::User)
}

echo 'Setting variables: GST_PLUGIN_PATH, Path (for DLLs)'
$CURRENT_DIR = (Get-Item .).FullName
[Environment]::SetEnvironmentVariable('GST_PLUGIN_PATH', "C:\gstreamer\1.0\msvc_x86_64\bin;C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0;$CURRENT_DIR", [System.EnvironmentVariableTarget]::User)
$USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
if (-Not [Environment]::GetEnvironmentVariable('Path', 'User').Contains($CURRENT_DIR)) {
	[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ';' + $CURRENT_DIR, [System.EnvironmentVariableTarget]::User)
}

echo 'Setting variables: GST_PLUGIN_SCANNER'
$GSTREAMER_PLUGIN_SCANNER_PATH = (Get-ChildItem -Filter gst-plugin-scanner.exe -Recurse -Path 'C:\gstreamer' -Include 'gst-plugin-scanner.exe' -ErrorAction SilentlyContinue) | Select-Object -First 1
[Environment]::SetEnvironmentVariable('GST_PLUGIN_SCANNER', $GSTREAMER_PLUGIN_SCANNER_PATH, [System.EnvironmentVariableTarget]::User)

if (-Not [Environment]::GetEnvironmentVariable('Path', 'User').Contains('gstreamer')) {
	echo 'Setting variables: Path (for gst-launch-1.0)'
	$GSTREAMER_DIR = (Get-ChildItem -Filter gst-launch-1.0.exe -Recurse -Path 'C:\gstreamer' -Include 'gst-launch-1.0.exe' -ErrorAction SilentlyContinue).DirectoryName | Select-Object -First 1
	$USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
	[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ';' + $GSTREAMER_DIR, [System.EnvironmentVariableTarget]::User)
}

echo 'Searching for OpenVINO'
if (-Not [System.IO.File]::Exists('C:\openvino\setupvars.ps1')) {
	echo 'OpenVINO C:\openvino\setupvars.ps1 not found - please install OpenVINO!'

} else {
	echo 'OpenVINO found'
}

echo 'Setting variables:, OpenVINO_DIR, OPENVINO_LIB_PATHS, Path (for OpenVINO)'
[Environment]::SetEnvironmentVariable('OpenVINO_DIR', "C:\openvino\runtime\cmake", [System.EnvironmentVariableTarget]::User)
[Environment]::SetEnvironmentVariable('OPENVINO_LIB_PATHS', "C:\openvino\runtime\3rdparty\tbb\bin;C:\openvino\runtime\bin\intel64\Release", [System.EnvironmentVariableTarget]::User)
if (-Not [Environment]::GetEnvironmentVariable('Path', 'User').Contains('openvino')) {
	$USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
	[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ";C:\openvino\runtime\3rdparty\tbb\bin;C:\openvino\runtime\bin\intel64\Release", [System.EnvironmentVariableTarget]::User)
}

$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')
$env:GST_PLUGIN_PATH = [System.Environment]::GetEnvironmentVariable('GST_PLUGIN_PATH','User')
$env:GST_PLUGIN_SCANNER = [System.Environment]::GetEnvironmentVariable('GST_PLUGIN_SCANNER','User')
$env:LIBVA_DRIVER_NAME = [System.Environment]::GetEnvironmentVariable('LIBVA_DRIVER_NAME','User')
$env:LIBVA_DRIVERS_PATH = [System.Environment]::GetEnvironmentVariable('LIBVA_DRIVERS_PATH','User')
$env:OpenVINO_DIR = [System.Environment]::GetEnvironmentVariable('OpenVINO_DIR','User')
$env:OPENVINO_LIB_PATHS = [System.Environment]::GetEnvironmentVariable('OPENVINO_LIB_PATHS','User')

echo "USER ENVIRONMENT VARIABLES"
echo "Path:"
$env:Path
echo "GST_PLUGIN_PATH:"
$env:GST_PLUGIN_PATH
echo "GST_PLUGIN_SCANNER:"
$env:GST_PLUGIN_SCANNER
echo "LIBVA_DRIVER_NAME:"
$env:LIBVA_DRIVER_NAME
echo "LIBVA_DRIVERS_PATH:"
$env:LIBVA_DRIVERS_PATH
echo "OpenVINO_DIR:"
$env:OpenVINO_DIR
echo "OPENVINO_LIB_PATHS:"
$env:OPENVINO_LIB_PATHS

echo ""
echo "Generating GStreamer cache. It may take up to a few minutes for the first run"
echo "Please wait for a moment... "

try {
	$(gst-inspect-1.0.exe gvadetect)
} catch {
	echo "Error caught - clearing a cache and retrying..."
	mv gstvideoanalytics.dll gstvideoanalytics.dll.old
	$(gst-inspect-1.0.exe gvadetect)
	mv gstvideoanalytics.dll.old gstvideoanalytics.dll
	$(gst-inspect-1.0.exe gvadetect)
}

echo "DLStreamer is ready"
