# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG DOCKER_PRIVATE_REGISTRY

ARG UBUNTU_VERSION=bionic-20220301
ARG BASE_IMAGE=${DOCKER_PRIVATE_REGISTRY}ubuntu:${UBUNTU_VERSION}
FROM ${BASE_IMAGE}
ARG UBUNTU_CODENAME=bionic
ARG PYTHON_VERSION=python3.6

LABEL Description="This is the runtime image of Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework for Ubuntu 18.04"
LABEL Vendor="Intel Corporation"

ARG INSTALL_RECOMMENDED_OPENCL_DRIVER=true
ARG INSTALL_RECOMMENDED_MEDIA_DRIVER=true
ARG INSTALL_KAFKA_CLIENT=true
ARG INSTALL_MQTT_CLIENT=true
ARG INSTALL_DPCPP=false

ARG DLSTREAMER_APT_VERSION="*"
ENV DLSTREAMER_DIR=/opt/intel/dlstreamer
ARG OPENVINO_VERSION=2022.1.0
ENV INTEL_OPENVINO_DIR=/opt/intel/openvino_2022

WORKDIR /root
USER root
SHELL ["/bin/bash", "-xo", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive

# Install curl and apt-key dependencies
RUN apt-get update && apt-get install -y -q --no-install-recommends curl gpg-agent software-properties-common

# Add public key
ARG INTEL_GPG_KEY=https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN curl -fsSL ${INTEL_GPG_KEY} | apt-key add -

# Add Intel® Graphics repository
ARG GRAPHICS_APT_REPO=https://repositories.intel.com/graphics
ARG GRAPHICS_KEY=${GRAPHICS_APT_REPO}/intel-graphics.key
ARG GRAPHICS_REPO="deb [arch=amd64] ${GRAPHICS_APT_REPO}/ubuntu ${UBUNTU_CODENAME} main"
RUN curl -fsSL ${GRAPHICS_KEY} | apt-key add -
RUN if [ "${GRAPHICS_REPO}" != "" ] ; then \
    echo "${GRAPHICS_REPO}" > /etc/apt/sources.list.d/intel-graphics.list ; \
    fi

# Add OpenVINO™ toolkit repository
ARG OPENVINO_APT_REPO=https://apt.repos.intel.com/openvino/2022
ARG OPENVINO_REPO="deb ${OPENVINO_APT_REPO} ${UBUNTU_CODENAME} main"
RUN if [ "${OPENVINO_REPO}" != "" ] ; then \
    echo "${OPENVINO_REPO}" > /etc/apt/sources.list.d/intel-openvino.list ; \
    fi

# Add Intel® DL Streamer repository
ARG DLSTREAMER_APT_REPO=https://apt.repos.intel.com/openvino/2022
ARG DLSTREAMER_APT_REPO_COMPONENT=main
ARG DLSTREAMER_REPO="deb ${DLSTREAMER_APT_REPO} ${UBUNTU_CODENAME} ${DLSTREAMER_APT_REPO_COMPONENT}"
RUN if [ "${DLSTREAMER_REPO}" != "${OPENVINO_REPO}" ] ; then \
    echo "${DLSTREAMER_REPO}" > /etc/apt/sources.list.d/intel-dlstreamer.list ; \
    fi

# Add Intel® oneAPI repository if INSTALL_DPCPP=true
ARG ONEAPI_APT_REPO=https://apt.repos.intel.com/oneapi
ARG ONEAPI_REPO="deb ${ONEAPI_APT_REPO} all main"
RUN if [ "$INSTALL_DPCPP" = "true" ] ; then \
    echo "${ONEAPI_REPO}" > /etc/apt/sources.list.d/intel-oneapi.list ; \
    fi

# If INSTALL_RECOMMENDED_OPENCL_DRIVER set to true, run script from openvino-dev package
RUN if [ "${INSTALL_RECOMMENDED_OPENCL_DRIVER}" = "true" ] ; then \
    apt-get update && apt-get install -y libopenvino-dev-${OPENVINO_VERSION} ; \
    ${INTEL_OPENVINO_DIR}/install_dependencies/install_NEO_OCL_driver.sh -y ; \
    apt-get remove -y libopenvino-dev-${OPENVINO_VERSION} ; \
    fi

# If INSTALL_RECOMMENDED_MEDIA_DRIVER set to true, run script from dlstreamer-env package
RUN if [ "${INSTALL_RECOMMENDED_MEDIA_DRIVER}" = "true" ] ; then \
    apt-get update && apt-get install -y intel-dlstreamer-env=${DLSTREAMER_APT_VERSION} ; \
    source ${DLSTREAMER_DIR}/install_dependencies/install_media_driver.sh ; \
    fi

# Install specific OpenCL driver version, if specified
ARG OPENCL_DRIVER_APT_VERSION=
RUN if [ "${OPENCL_DRIVER_APT_VERSION}" != "" ] ; then \
    apt-get update && apt-get install -y intel-opencl-icd=${OPENCL_DRIVER_APT_VERSION} ; \
    fi

# Install specific media driver version, if specified
ARG MEDIA_DRIVER_APT_VERSION=
RUN if [ "${MEDIA_DRIVER_APT_VERSION}" != "" ] ; then \
    apt-get update && apt-get install -y intel-media-va-driver-non-free=${MEDIA_DRIVER_APT_VERSION} ; \
    fi

# Install Intel® DL Streamer runtime package and Python bindings
RUN apt-get update && apt-get install -y intel-dlstreamer=${DLSTREAMER_APT_VERSION}
RUN apt-get update && apt-get install -y python3-intel-dlstreamer=${DLSTREAMER_APT_VERSION}

# If INSTALL_DPCPP set to true, install Intel® DL Streamer package with DPC++ based elements
ARG DPCPP_APT_VERSION=*
RUN if [ "${INSTALL_DPCPP}" = "true" ] ; then \
    apt-get update && apt-get install -y intel-oneapi-compiler-dpcpp-cpp-runtime=${DPCPP_APT_VERSION}; \
    apt-get update && apt-get install -y intel-dlstreamer-dpcpp=${DLSTREAMER_APT_VERSION}; \
    fi

ARG INSTALL_METAPUBLISH_DEPENDENCIES=
# If INSTALL_KAFKA_CLIENT set to true, run post-install script from DL Streamer
RUN if [ "${INSTALL_KAFKA_CLIENT}" = "true" ] || [ "${INSTALL_METAPUBLISH_DEPENDENCIES}" = "true" ] ; then \
    ${DLSTREAMER_DIR}/install_dependencies/install_kafka_client.sh ; \
    fi

# If INSTALL_MQTT_CLIENT set to true, run post-install script from DL Streamer
RUN if [ "${INSTALL_MQTT_CLIENT}" = "true" ] || [ "${INSTALL_METAPUBLISH_DEPENDENCIES}" = "true" ] ; then \
    ${DLSTREAMER_DIR}/install_dependencies/install_mqtt_client.sh ; \
    fi

# Install additional OpenVINO™ toolkit plugins
RUN apt-get update && apt-get install -y openvino-libraries

# Remove Intel® Graphics APT repository
RUN rm -f /etc/ssl/certs/Intel*
#RUN rm /etc/apt/sources.list.d/intel.list
RUN rm -rf /etc/apt/sources.list.d/intel-graphics.list
RUN apt-get update

ARG DLS_HOME=/home/dlstreamer

# Set environment
ENV HDDL_INSTALL_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl"
ENV TBB_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/cmake"

ENV OpenVINO_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV InferenceEngine_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV ngraph_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV OpenCV_DIR="$INTEL_OPENVINO_DIR/extras/opencv/cmake"

ENV LD_LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib:$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/extras/opencv/lib/:$INTEL_OPENVINO_DIR/runtime/lib/intel64:/usr/lib:$LD_LIBRARY_PATH"
ENV PYTHONPATH="$INTEL_OPENVINO_DIR/python/${PYTHON_VERSION}:$INTEL_OPENVINO_DIR/extras/opencv/python:$PYTHONPATH"

ENV LIBDIR="${DLSTREAMER_DIR}/lib"
ENV GST_PLUGIN_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib/gstreamer-1.0:${GST_PLUGIN_PATH}"
ENV LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib:/usr/lib:${LIBRARY_PATH}"
ENV PKG_CONFIG_PATH="${DLSTREAMER_DIR}/lib/pkgconfig:${DLSTREAMER_DIR}/gstreamer/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
ENV MODELS_PATH="${MODELS_PATH:-${DLS_HOME}/intel/dl_streamer/models}"
ENV LC_NUMERIC="C"
ENV LIBVA_DRIVER_NAME="iHD"

ENV PATH="${DLSTREAMER_DIR}/gstreamer/bin:${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0:${PATH}"

ENV GI_TYPELIB_PATH="${DLSTREAMER_DIR}/gstreamer/lib/girepository-1.0"
ENV GST_PLUGIN_SCANNER="${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0/gst-plugin-scanner"

RUN useradd -ms /bin/bash -u 1000 -G video dlstreamer
WORKDIR ${DLS_HOME}
USER dlstreamer
COPY ./docker/third-party-programs.txt ${DLSTREAMER_DIR}/

CMD ["/bin/bash"]
