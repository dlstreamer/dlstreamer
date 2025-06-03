# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG BASE_IMAGE
FROM ${BASE_IMAGE}

ARG DLSTREAMER_VERSION=2025.0.1.3
ARG DLSTREAMER_BUILD_NUMBER

# hadolint ignore=DL3002
USER root
ENV USER=dlstreamer
ENV RPM_PKG_NAME=intel-dlstreamer-${DLSTREAMER_VERSION}

RUN \
    dnf install -y rpmdevtools patchelf && \
    dnf clean all

RUN \
    mkdir -p /${RPM_PKG_NAME}/opt/intel/ && \
    mkdir -p /${RPM_PKG_NAME}/opt/opencv/include && \
    mkdir -p /${RPM_PKG_NAME}/opt/openh264/ && \
    mkdir -p /${RPM_PKG_NAME}/opt/rdkafka && \
    mkdir -p /${RPM_PKG_NAME}/opt/ffmpeg && \
    mkdir -p /${RPM_PKG_NAME}/usr/local/lib/ && \
    mkdir -p /${RPM_PKG_NAME}/usr/lib/ && \
    cp -r "${DLSTREAMER_DIR}" /${RPM_PKG_NAME}/opt/intel/dlstreamer && \
    cp -rT "${GSTREAMER_DIR}" /${RPM_PKG_NAME}/opt/intel/dlstreamer/gstreamer && \
    cp /usr/lib64/libopencv*.so.410 /${RPM_PKG_NAME}/opt/opencv/ && \
    cp /usr/local/lib64/libopencv*.so.410 /${RPM_PKG_NAME}/opt/opencv/ && \
    cp "${GSTREAMER_DIR}"/lib/libopenh264.so /${RPM_PKG_NAME}/opt/openh264/libopenh264.so.7 && \
    cp /usr/local/lib/librdkafka++.so /${RPM_PKG_NAME}/opt/rdkafka/librdkafka++.so.1 && \
    cp /usr/local/lib/librdkafka.so /${RPM_PKG_NAME}/opt/rdkafka/librdkafka.so.1 && \
    find /usr/local/lib -regextype grep -regex ".*libav.*so\.[0-9]*$" -exec cp {} /${RPM_PKG_NAME}/opt/ffmpeg \; && \
    find /usr/local/lib -regextype grep -regex ".*libswscale.*so\.[0-9]*$" -exec cp {} /${RPM_PKG_NAME}/opt/ffmpeg \; && \
    find /usr/local/lib -regextype grep -regex ".*libswresample.*so\.[0-9]*$" -exec cp {} /${RPM_PKG_NAME}/opt/ffmpeg \; && \
    cp "${GSTREAMER_DIR}"/lib/libvorbis* /${RPM_PKG_NAME}/usr/local/lib/ && \
    cp /usr/local/lib/libpaho* /${RPM_PKG_NAME}/usr/local/lib/ && \
    cp /usr/local/lib/libav* /${RPM_PKG_NAME}/usr/local/lib/ && \
    cp "${GSTREAMER_DIR}"/lib/libgst* /${RPM_PKG_NAME}/usr/lib && \
    cp -r "${GSTREAMER_DIR}"/lib/gstreamer-1.0/ /${RPM_PKG_NAME}/usr/lib/ && \
    cp -r /usr/local/include/opencv4/* /${RPM_PKG_NAME}/opt/opencv/include && \
    cp "${DLSTREAMER_DIR}"/LICENSE /${RPM_PKG_NAME}/ && \
    rm -rf /${RPM_PKG_NAME}/opt/intel/dlstreamer/archived && \
    rm -rf /${RPM_PKG_NAME}/opt/intel/dlstreamer/docker && \
    rm -rf /${RPM_PKG_NAME}/opt/intel/dlstreamer/docs && \
    rm -rf /${RPM_PKG_NAME}/opt/intel/dlstreamer/infrastructure && \
    rm -rf /${RPM_PKG_NAME}/opt/intel/dlstreamer/tests && \
    rpmdev-setuptree && \
    tar -czf ~/rpmbuild/SOURCES/${RPM_PKG_NAME}.tar.gz -C / ${RPM_PKG_NAME} && \
    cp "${DLSTREAMER_DIR}"/docker/onebinary/fedora41/intel-dlstreamer.spec ~/rpmbuild/SPECS/ && \
    sed -i -e "s/DLSTREAMER_VERSION/${DLSTREAMER_VERSION}/g" ~/rpmbuild/SPECS/intel-dlstreamer.spec && \
    sed -i -e "s/CURRENT_DATE_TIME/$(date '+%a %b %d %Y')/g" ~/rpmbuild/SPECS/intel-dlstreamer.spec && \
    rpmbuild -bb ~/rpmbuild/SPECS/intel-dlstreamer.spec

RUN cp ~/rpmbuild/RPMS/x86_64/${RPM_PKG_NAME}* /${RPM_PKG_NAME}.${DLSTREAMER_BUILD_NUMBER}-1.fc41.x86_64.rpm
