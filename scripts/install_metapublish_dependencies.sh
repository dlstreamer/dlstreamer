#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

CURDIR=$PWD
cd /tmp/ || exit

if [ -n "$(command -v apt-get)" ]
then
    apt-get update && apt-get install -y --no-install-recommends \
        uuid uuid-dev libssl-dev gcc g++ make curl ca-certificates
elif [ -n "$(command -v yum)" ]
then
    yum check-update; yum install -y uuid libuuid-devel openssl-devel gcc gcc-c++ make curl ca-certificates
else
    echo "ERROR: Neither yum nor apt-get found"
    exit
fi

curl -sSL https://github.com/edenhill/librdkafka/archive/v1.5.0.tar.gz | tar -xz
cd librdkafka-1.5.0 || exit
./configure
make
make install
cd ..
rm -rf librdkafka-1.5.0

# remove symbolic links from previous installation
if [[ -L "/usr/local/lib/libpaho-mqtt3c.so" ]]; then unlink /usr/local/lib/libpaho-mqtt3c.so; fi
if [[ -L "/usr/local/lib/libpaho-mqtt3cs.so" ]]; then unlink /usr/local/lib/libpaho-mqtt3cs.so; fi
if [[ -L "/usr/local/lib/libpaho-mqtt3as.so" ]]; then unlink /usr/local/lib/libpaho-mqtt3as.so; fi
if [[ -L "/usr/local/lib/libpaho-mqtt3a.so" ]]; then unlink /usr/local/lib/libpaho-mqtt3a.so; fi

curl -sSL https://github.com/eclipse/paho.mqtt.c/archive/v1.3.4.tar.gz | tar -xz
cd paho.mqtt.c-1.3.4 || exit
make
make install
cd ..
rm -rf paho.mqtt.c-1.3.4

cd "$CURDIR" || return
