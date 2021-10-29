#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

CURDIR=$PWD
cd /tmp/

if [ -n "$(command -v apt-get)" ]
then
    apt-get update && apt-get install -y --no-install-recommends \
        uuid uuid-dev libssl-dev gcc g++ make wget ca-certificates
elif [ -n "$(command -v yum)" ]
then
    yum check-update; yum install -y uuid libuuid-devel openssl-devel gcc gcc-c++ make wget ca-certificates
else
    echo "ERROR: Neither yum nor apt-get found"
    exit -1
fi

wget -O - https://github.com/edenhill/librdkafka/archive/v1.5.0.tar.gz | tar -xz
cd librdkafka-1.5.0
./configure
make
make install
cd ..
rm -rf librdkafka-1.5.0

wget -O - https://github.com/eclipse/paho.mqtt.c/archive/v1.3.4.tar.gz | tar -xz
cd paho.mqtt.c-1.3.4
make
make install
cd ..
rm -rf paho.mqtt.c-1.3.4

cd $CURDIR
