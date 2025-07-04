# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

if (-Not (Test-Path 'C:\gstreamer')) {
	echo 'Please install GStreamer to folder C:\gstreamer and then run the script again.'
	echo 'Link: https://gstreamer.freedesktop.org/data/pkg/windows/1.26.2/msvc/gstreamer-1.0-msvc-x86_64-1.26.2.msi'
	exit
}

if (-Not [Environment]::GetEnvironmentVariable('LIBVA_DRIVER_NAME', 'User')) {
	$NUGET_PATH = (Get-Item .).FullName + '/nuget.exe'
	if (-Not [System.IO.File]::Exists($NUGET_PATH)) {
        curl -Uri https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile nuget.exe
		Start-Process -NoNewWindow -FilePath $NUGET_PATH -ArgumentList 'install', 'Microsoft.Direct3D.VideoAccelerationCompatibilityPack'
	}
	[Environment]::SetEnvironmentVariable('LIBVA_DRIVER_NAME', 'vaon12', [System.EnvironmentVariableTarget]::User)
	[Environment]::SetEnvironmentVariable('LIBVA_DRIVERS_PATH', (Get-Item .).FullName + '\Microsoft.Direct3D.VideoAccelerationCompatibilityPack.1.0.2\build\native\x64\bin\', [System.EnvironmentVariableTarget]::User)
    $USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
	if (-Not $USER_PATH.Contains('VideoAccelerationCompatibilityPack')) {
		[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ';' + [Environment]::GetEnvironmentVariable('LIBVA_DRIVERS_PATH', 'User'), [System.EnvironmentVariableTarget]::User)
	}
}

if (-Not [Environment]::GetEnvironmentVariable('GST_PLUGIN_PATH', 'User')) {
    $CURRENT_DIR = (Get-Item .).FullName
	[Environment]::SetEnvironmentVariable('GST_PLUGIN_PATH', $CURRENT_DIR, [System.EnvironmentVariableTarget]::User)
}

if (-Not [Environment]::GetEnvironmentVariable('GST_PLUGIN_SCANNER', 'User')) {
	echo 'Searching for gst-plugin-scanner.exe'
	$GSTREAMER_PLUGIN_SCANNER_PATH = (Get-ChildItem -Filter gst-plugin-scanner.exe -Recurse -Path 'C:\gstreamer' -Include 'gst-plugin-scanner.exe' -ErrorAction SilentlyContinue) | Select-Object -First 1
	[Environment]::SetEnvironmentVariable('GST_PLUGIN_SCANNER', $GSTREAMER_PLUGIN_SCANNER_PATH, [System.EnvironmentVariableTarget]::User)
}

if (-Not [Environment]::GetEnvironmentVariable('Path', 'User').Contains('gstreamer')) {
	echo 'Searching for gst-launch-1.0.exe'
	$GSTREAMER_DIR = (Get-ChildItem -Filter gst-launch-1.0.exe -Recurse -Path 'C:\gstreamer' -Include 'gst-launch-1.0.exe' -ErrorAction SilentlyContinue).DirectoryName | Select-Object -First 1
	$USER_PATH = [Environment]::GetEnvironmentVariable('Path', 'User')
	[Environment]::SetEnvironmentVariable('Path', $USER_PATH + ';' + $GSTREAMER_DIR, [System.EnvironmentVariableTarget]::User)
}

$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [System.Environment]::GetEnvironmentVariable('Path','User')

$env:GST_PLUGIN_PATH = [System.Environment]::GetEnvironmentVariable('GST_PLUGIN_PATH','User')
$env:GST_PLUGIN_SCANNER = [System.Environment]::GetEnvironmentVariable('GST_PLUGIN_SCANNER','User')

echo 'Searching for OpenVINO'
if (-Not [System.IO.File]::Exists('C:\openvino\setupvars.ps1')) {
	echo 'Installing OpenVINO'
    curl -Uri https://storage.openvinotoolkit.org/repositories/openvino/packages/2025.1/windows/openvino_toolkit_windows_2025.1.0.18503.6fec06580ab_x86_64.zip -OutFile openvino.zip
	Expand-Archive -LiteralPath 'openvino.zip' -DestinationPath 'C:\'
	mv C:\openvino_toolkit_windows_2025.1.0.18503.6fec06580ab_x86_64 C:\openvino
	rm openvino.zip
} else {
	echo 'OpenVINO found'
}

& C:\openvino\setupvars.ps1

echo ""
echo "Generating GStreamer cache. It may take up to a few minutes for the first run"
echo "Please wait for a moment... "

$errOutput = $( $output = & gst-inspect-1.0.exe gvadetect ) 2>&1

if ($errOutput[0].ToString().Contains("No such element or plugin")) {
	mv gstvideoanalytics.dll gstvideoanalytics.dll.old
	$errOutput = $( $output = & gst-inspect-1.0.exe gvadetect ) 2>&1
	mv gstvideoanalytics.dll.old gstvideoanalytics.dll
}

echo "DLStreamer is ready"