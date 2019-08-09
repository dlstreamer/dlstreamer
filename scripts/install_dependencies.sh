#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

CURDIR=$PWD
cd /tmp/

wget -O - https://github.com/Intel-Media-SDK/MediaSDK/releases/download/MediaSDK-2018-Q2.2/MediaStack.tar.gz | tar xz
cd MediaStack && sudo ./install_media.sh
cd ..

sudo apt update && sudo apt install -y --no-install-recommends \
    autoconf libtool libdrm-dev xorg xorg-dev openbox libx11-dev \
    libgl1-mesa-glx libgl1-mesa-dev \
    libgstreamer1.0-0 gstreamer1.0-plugins-base libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-bad gstreamer1.0-libav \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-tools \
    libgstreamer-plugins-bad1.0-dev libva-dev gstreamer1.0-vaapi

cd $CURDIR
