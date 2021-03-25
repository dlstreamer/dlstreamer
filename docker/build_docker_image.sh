#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

: ${1?"Usage: $0 OPENVINO_ARCHIV_URL IMAGE_TAG DOCKER_PRIVATE_REGISTRY_PREFIX"}
openvino_url=$1
tag=${2:-latest}
docker_private_registry=$3

if wget --spider "$openvino_url" 2>/dev/null; then
  openvino_ver=$(echo "$openvino_url" | grep -Eo '[0-9]{4}.[0-9].[0-9]{1,4}' | grep -v '/')
  if [[ -n ${openvino_ver} ]]; then
      echo "OpenVINO™ Toolkit version is $openvino_ver"
  else
      echo "Cannot find OpenVINO™ Toolkit version from URL: $openvino_url"  && exit 1
  fi
else
  echo "Cannot find OpenVINO™ Toolkit archive from URL: $openvino_url" && exit 1
fi

BASEDIR="$(dirname "$(readlink -fm "$0")")"
CONTEXTDIR="$(dirname "$BASEDIR")"
dockerfile="Dockerfile"

docker build -f ${BASEDIR}/${dockerfile} -t dl_streamer:"$tag" \
    --build-arg http_proxy=${HTTP_PROXY} \
    --build-arg https_proxy=${HTTPS_PROXY} \
    --build-arg DOCKER_PRIVATE_REGISTRY=${docker_private_registry} \
    --build-arg OPENVINO_URL=${openvino_url} \
    --build-arg OpenVINO_VERSION=${openvino_ver} \
    $CONTEXTDIR
