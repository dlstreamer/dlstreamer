# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG DOCKER_PRIVATE_REGISTRY

ARG UBUNTU_VERSION=20.04
ARG BASE_IMAGE=${DOCKER_PRIVATE_REGISTRY}ubuntu:${UBUNTU_VERSION}
FROM ${BASE_IMAGE}
ARG UBUNTU_CODENAME=focal
ARG GRAPHICS_DISTRIBUTION=focal-legacy
ARG PYTHON_VERSION=python3.8

LABEL description="This is the runtime image of Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework for Ubuntu ${UBUNTU_VERSION}"
LABEL vendor="Intel Corporation"

ARG INSTALL_RECOMMENDED_OPENCL_DRIVER=false
ARG INSTALL_RECOMMENDED_MEDIA_DRIVER=false
ARG INSTALL_KAFKA_CLIENT=true
ARG INSTALL_MQTT_CLIENT=true
ARG INSTALL_DPCPP=true

ARG DLSTREAMER_APT_VERSION="*"
ENV DLSTREAMER_DIR=/opt/intel/dlstreamer
ENV INTEL_OPENVINO_DIR=/opt/intel/openvino_2022

WORKDIR /root
USER root
SHELL ["/bin/bash", "-xo", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive

# Install curl and apt-key dependencies
RUN apt-get update && apt-get install -y -q  --no-install-recommends curl gpg-agent software-properties-common  \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Add public key
ARG INTEL_GPG_KEY=https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN curl -fsSL ${INTEL_GPG_KEY} | apt-key add -

# Add Intel® Graphics repository
ARG GRAPHICS_APT_REPO=https://repositories.intel.com/graphics
ARG GRAPHICS_KEY=${GRAPHICS_APT_REPO}/intel-graphics.key
ARG GRAPHICS_REPO="deb [arch=amd64] ${GRAPHICS_APT_REPO}/ubuntu ${GRAPHICS_DISTRIBUTION} main"
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

# If INSTALL_RECOMMENDED_MEDIA_DRIVER set to true, run script from dlstreamer-env package
RUN if [ "${INSTALL_RECOMMENDED_MEDIA_DRIVER}" = "true" ] ; then \
    apt-get update && apt-get install -y intel-dlstreamer-env=${DLSTREAMER_APT_VERSION} ; \
    ${DLSTREAMER_DIR}/install_dependencies/install_media_driver.sh && apt-get clean && rm -rf /var/lib/apt/lists/* ; \
    fi

# Install specific OpenCL driver version, if specified
ARG OPENCL_DRIVER_APT_VERSION=
RUN if [ "${OPENCL_DRIVER_APT_VERSION}" != "" ] ; then \
    apt-get update && apt-get install -y intel-opencl-icd=${OPENCL_DRIVER_APT_VERSION} && apt-get clean && rm -rf /var/lib/apt/lists/* ; \
    fi

# Install specific media driver version, if specified
ARG MEDIA_DRIVER_APT_VERSION=
RUN if [ "${MEDIA_DRIVER_APT_VERSION}" != "" ] ; then \
    apt-get update && apt-get install -y intel-media-va-driver-non-free=${MEDIA_DRIVER_APT_VERSION} && apt-get clean && rm -rf /var/lib/apt/lists/* ; \
    fi

# Install Intel® DL Streamer runtime package and Python bindings
RUN apt-get update && apt-get install -y intel-dlstreamer=${DLSTREAMER_APT_VERSION} python3-intel-dlstreamer=${DLSTREAMER_APT_VERSION} \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install OpenVINO™ toolkit
ARG OPENVINO_INSTALL_OPTIONS=
RUN ${DLSTREAMER_DIR}/install_dependencies/install_openvino.sh ${OPENVINO_INSTALL_OPTIONS} 

# If PYTHON_VERSION=python3.9, install it and create link for gstgva module
RUN if [ "${PYTHON_VERSION}" = "python3.9" ] ; then \
    ${DLSTREAMER_DIR}/install_dependencies/install_python3.9.sh ; \
    python3 -m pip install --no-cache-dir numpy PyGObject ; \
    ln -s /usr/lib/python3/dist-packages/gstgva /usr/local/lib/python3.9/ ; \
    fi

# Install numpy via pip
RUN python3 -m pip install --no-cache-dir --force-reinstall numpy

# If INSTALL_RECOMMENDED_OPENCL_DRIVER set to true, run OpenVINO script
RUN if [ "${INSTALL_RECOMMENDED_OPENCL_DRIVER}" = "true" ] ; then \
    ${INTEL_OPENVINO_DIR}/install_dependencies/install_NEO_OCL_driver.sh -y ; \
    fi

# If INSTALL_DPCPP set to true, install Intel® DL Streamer package with DPC++ based elements
ARG DPCPP_APT_VERSION=*
RUN if [ "${INSTALL_DPCPP}" = "true" ] ; then \
    apt-get update && apt-get install -y intel-oneapi-compiler-dpcpp-cpp-runtime=${DPCPP_APT_VERSION} intel-dlstreamer-dpcpp=${DLSTREAMER_APT_VERSION} && \
    apt-get clean && rm -rf /var/lib/apt/lists/* ; \
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

ARG DLS_HOME=/home/dlstreamer

# OpenVINO environment variables
ENV OpenVINO_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV InferenceEngine_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV ngraph_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV HDDL_INSTALL_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl"
ENV TBB_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/cmake"
ENV LD_LIBRARY_PATH="$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/runtime/lib/intel64:$LD_LIBRARY_PATH"
ENV PYTHONPATH="$INTEL_OPENVINO_DIR/python/${PYTHON_VERSION}:$PYTHONPATH"

# DL Streamer environment variables
ENV GSTREAMER_DIR=${DLSTREAMER_DIR}/gstreamer
ENV GST_PLUGIN_PATH="${DLSTREAMER_DIR}/lib/gstreamer-1.0:${GSTREAMER_DIR}/lib/gstreamer-1.0:${GST_PLUGIN_PATH}"
ENV LIBRARY_PATH="/usr/lib:${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/lib/gstreamer-1.0:${LIBRARY_PATH}"
ENV LD_LIBRARY_PATH="/usr/lib:${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/lib/gstreamer-1.0:${LD_LIBRARY_PATH}"
ENV PKG_CONFIG_PATH="/usr/lib/x86_64-linux-gnu/pkgconfig:${DLSTREAMER_DIR}/lib/pkgconfig:${PKG_CONFIG_PATH}"
ENV MODELS_PATH="${MODELS_PATH:-${DLS_HOME}/intel/dl_streamer/models}"
ENV LC_NUMERIC="C"

# if USE_CUSTOM_GSTREAMER set, add GStreamer build to GST_PLUGIN_SCANNER and PATH
ARG USE_CUSTOM_GSTREAMER=yes
ENV GST_PLUGIN_SCANNER=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/bin/gstreamer-1.0/gst-plugin-scanner}
ENV GI_TYPELIB_PATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/girepository-1.0}
ENV PATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/bin:}${PATH}
ENV PKG_CONFIG_PATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/pkgconfig:}${PKG_CONFIG_PATH}
ENV LIBRARY_PATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib:}$LIBRARY_PATH
ENV LD_LIBRARY_PATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib:}$LD_LIBRARY_PATH
ENV PYTHONPATH=${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/python3/dist-packages:}$PYTHONPATH

# DPC++ runtime
ENV DPCPP_DIR="/opt/intel/oneapi/compiler/latest/linux"
ENV PATH="${PATH}:${DPCPP_DIR}/lib:${DPCPP_DIR}/compiler/lib/intel64_lin"
ENV LIBRARY_PATH="${LIBRARY_PATH}:${DPCPP_DIR}/lib:${DPCPP_DIR}/compiler/lib/intel64_lin"
ENV LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${DPCPP_DIR}/lib:${DPCPP_DIR}/lib/x64:${DPCPP_DIR}/compiler/lib/intel64_lin"

COPY ./docker/third-party-programs.txt ${DLSTREAMER_DIR}/

RUN useradd -ms /bin/bash -u 1000 -G video dlstreamer

# Remove Intel® Graphics APT repository
RUN mv /etc/apt/sources.list.d/intel-graphics.list ${DLS_HOME}/
RUN rm -f /etc/ssl/certs/Intel*
RUN apt-get update \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR ${DLS_HOME}
USER dlstreamer
CMD ["/bin/bash"]
