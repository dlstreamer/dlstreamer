# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BASEDIR=$(dirname "$0")/..

[ ! -d "${BASEDIR}/build" ] && mkdir ${BASEDIR}/build
cd ${BASEDIR}/build
cmake ..
make -j $(nproc)
