#!/bin/bash
# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# shellcheck source=/dev/null
. /etc/os-release

if [ "$ID" != "ubuntu" ]; then
    echo "Unsupported system: $ID $VERSION_ID"
    exit 1
fi

apt-get update && apt-get install -y --no-install-recommends \
        libssl-dev libusb-1.0-0-dev libudev-dev pkg-config libgtk-3-dev

git clone https://github.com/IntelRealSense/librealsense.git
cd librealsense || exit
mkdir build && cd build || exit
cmake ../ -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=false -DBUILD_GRAPHICAL_EXAMPLES=false
make -j
make install
