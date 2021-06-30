#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

CURDIR=$PWD
cd /tmp/

apt update && apt install -y --no-install-recommends \
    uuid uuid-dev libssl-dev

wget -O - https://github.com/eclipse/paho.mqtt.c/archive/v1.3.4.tar.gz | tar -xz
cd paho.mqtt.c-1.3.4
make
make install
cd ..
rm -rf paho.mqtt.c-1.3.4

wget -O - https://github.com/edenhill/librdkafka/archive/v1.5.0.tar.gz | tar -xz
cd librdkafka-1.5.0
./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu/
make
make install
cd ..
rm -rf librdkafka-1.5.0

cd $CURDIR
