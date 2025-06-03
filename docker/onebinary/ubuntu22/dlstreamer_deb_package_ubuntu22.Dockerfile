# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG DLSTREAMER_VERSION=2025.0.1.3
ARG DLSTREAMER_BUILD_NUMBER

ARG DEBIAN_FRONTEND=noninteractive

# hadolint ignore=DL3002
USER root
ENV USER=dlstreamer

RUN apt-get update && \
    apt-get install -y --no-install-recommends devscripts=\* dh-make=\* && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN \
    mkdir -p /deb-pkg/opt/intel/ && \
    mkdir -p /deb-pkg/opt/opencv/include && \
    mkdir -p /deb-pkg/opt/openh264/ && \
    mkdir -p /deb-pkg/opt/rdkafka && \
    mkdir -p /deb-pkg/opt/ffmpeg && \
    mkdir -p /deb-pkg/usr/local/lib/ && \
    mkdir -p /deb-pkg/usr/lib/ && \
    cp -r ${DLSTREAMER_DIR} /deb-pkg/opt/intel/dlstreamer && \
    cp -rT ${GSTREAMER_DIR} /deb-pkg/opt/intel/dlstreamer/gstreamer && \
    cp /usr/local/lib/libopencv*.so.410 /deb-pkg/opt/opencv/ && \
    cp ${GSTREAMER_DIR}/lib/libopenh264.so /deb-pkg/opt/openh264/libopenh264.so.7 && \
    cp /usr/local/lib/librdkafka++.so /deb-pkg/opt/rdkafka/librdkafka++.so.1 && \
    cp /usr/local/lib/librdkafka.so /deb-pkg/opt/rdkafka/librdkafka.so.1 && \
    find /usr/local/lib -regextype grep -regex ".*libav.*so\.[0-9]*$" -exec cp {} /deb-pkg/opt/ffmpeg \; && \
    find /usr/local/lib -regextype grep -regex ".*libswscale.*so\.[0-9]*$" -exec cp {} /deb-pkg/opt/ffmpeg \; && \
    find /usr/local/lib -regextype grep -regex ".*libswresample.*so\.[0-9]*$" -exec cp {} /deb-pkg/opt/ffmpeg \; && \
    cp ${GSTREAMER_DIR}/lib/libvorbis* /deb-pkg/usr/local/lib/ && \
    cp /usr/local/lib/libpaho* /deb-pkg/usr/local/lib/ && \
    cp /usr/local/lib/libav* /deb-pkg/usr/local/lib/ && \
    cp ${GSTREAMER_DIR}/lib/libgst* /deb-pkg/usr/lib && \
    cp -r ${GSTREAMER_DIR}/lib/gstreamer-1.0/ /deb-pkg/usr/lib/ && \
    cp -r /usr/local/include/opencv4/* /deb-pkg/opt/opencv/include && \
    rm -rf /deb-pkg/opt/intel/dlstreamer/archived && \
    rm -rf /deb-pkg/opt/intel/dlstreamer/docker && \
    rm -rf /deb-pkg/opt/intel/dlstreamer/docs && \
    rm -rf /deb-pkg/opt/intel/dlstreamer/infrastructure && \
    rm -rf /deb-pkg/opt/intel/dlstreamer/tests

COPY docker/onebinary/debian /deb-pkg/debian

RUN \
    sed -i -e "s/noble/jammy/g" /deb-pkg/debian/changelog && \
    sed -i -e "s/DLSTREAMER_VERSION/${DLSTREAMER_VERSION}/g" /deb-pkg/debian/changelog && \
    sed -i -e "s/CURRENT_DATE_TIME/$(date -R)/g" /deb-pkg/debian/changelog && \
    sed -i -e "s/DLSTREAMER_VERSION/${DLSTREAMER_VERSION}/g" /deb-pkg/debian/control

WORKDIR /deb-pkg

RUN \
    debuild -z1 -us -uc

RUN mv /intel-dlstreamer_${DLSTREAMER_VERSION}_amd64.deb /intel-dlstreamer_${DLSTREAMER_VERSION}.${DLSTREAMER_BUILD_NUMBER}_amd64.deb