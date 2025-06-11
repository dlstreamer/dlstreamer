#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

#Uninstall DLStreamer
echo "Removing intel-dlstreamer package..."
sudo apt remove -y intel-dlstreamer
echo " Intel-dlstreamer package uninstalled..."
echo "Checking if there is any DLStreamer package installed"
installed_packages=$(sudo apt list --installed 2>/dev/null | grep dlstreamer || true)
echo " Found packages: $installed_packages"

#Uninstall openvino packages
echo "Uninstall openvino packages"
sudo apt remove -y openvino* libopenvino* python3-openvino*
sudo apt-get autoremove -y
echo "Packages are uninstalled"

sudo rm -rf /etc/apt/sources.list.d/intel-openvino*.list
sudo rm -rf /etc/apt/sources.list.d/sed.list

if [ -n "$installed_packages" ]; then
    echo "Found the following dlstreamer packages installed:"
    echo "$installed_packages"
    exit 1
else
    echo "No dlstreamer packages found."
    exit 0
fi
