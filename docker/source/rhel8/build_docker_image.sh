#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

: ${1?"Usage: $0 ACTIVATED_RHEL_IMAGE [IMAGE_TAG]"}
activated_rhel=$1
tag=${2:-latest}

BASEDIR="$(dirname "$(readlink -fm "$0")")"
dockerfile="Dockerfile"

docker build -f ${BASEDIR}/${dockerfile} -t dlstreamer:"$tag" \
    --build-arg http_proxy=${http_proxy} \
    --build-arg https_proxy=${https_proxy} \
    --build-arg no_proxy=${no_proxy} \
    --build-arg ACTIVATED_IMAGE=${activated_rhel} \
    $BASEDIR
