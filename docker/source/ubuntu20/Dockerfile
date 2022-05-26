# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG DOCKER_PRIVATE_REGISTRY

FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04

LABEL Description="This is the development image of Intel® Deep Learning Streamer Pipeline Framework (Intel® DL Streamer Pipeline Framework) for Ubuntu 20.04 LTS"
LABEL Vendor="Intel Corporation"

WORKDIR /root
USER root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

# Prerequisites
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
    cmake \
    build-essential \
    sudo \
    curl \
    numactl \
    gnupg2 \
    git \
    software-properties-common \
    pkg-config \
    libva-dev \
    python-gi-dev \
    vim \
    wget

# Register Intel® Graphics repository
ARG GRAPHICS_APT_REPO=https://repositories.intel.com/graphics/
RUN curl ${GRAPHICS_APT_REPO}intel-graphics.key | \
    apt-key add - && \
    apt-add-repository "deb [arch=amd64] ${GRAPHICS_APT_REPO}ubuntu/ focal main"

# Install Intel® Media Driver for VAAPI
ARG MEDIA_DRIVER_VERSION=21.4.1+i643~u20.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --allow-downgrades --no-install-recommends intel-media-va-driver-non-free=${MEDIA_DRIVER_VERSION}

# Install GPG key
ARG GPG_PUBLIC_KEY=https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB
RUN curl -sSL ${GPG_PUBLIC_KEY} | apt-key add -

# Register DL Streamer repository
ARG DLSTREAMER_APT_REPO=https://apt.repos.intel.com/openvino/2022
ARG DLSTREAMER_APT_REPO_COMPONENT=main
RUN sh -c "echo 'deb ${DLSTREAMER_APT_REPO} focal ${DLSTREAMER_APT_REPO_COMPONENT}' >> /etc/apt/sources.list"

# Install GStreamer
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y intel-dlstreamer-gst intel-dlstreamer-gst-vaapi

# Install Intel® OpenVINO
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y openvino openvino-opencv && /opt/intel/openvino_2022/install_dependencies/install_openvino_dependencies.sh -y

# Install python modules
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python3-pip && pip3 install --upgrade pip
ARG EXTRA_PYPI_INDEX_URL
RUN if [ -n "$EXTRA_PYPI_INDEX_URL" ] ; then \
    python3 -m pip config set global.extra-index-url ${EXTRA_PYPI_INDEX_URL} ;\
    fi
RUN python3 -m pip install openvino-dev[onnx,tensorflow2,pytorch,mxnet]

# Add local user
RUN useradd -ms /bin/bash -u 1000 -G video dlstreamer
ARG DLS_HOME=/home/dlstreamer
WORKDIR ${DLS_HOME}

# Install DL Streamer
COPY . ./dlstreamer/
RUN cd ./dlstreamer \
    && python3 -m pip install --no-cache-dir -r requirements.txt

# Install Intel® oneAPI DPC++/C++ Compiler
ARG ENABLE_DPCPP_INSTALLATION=OFF
RUN if [[ "$ENABLE_DPCPP_INSTALLATION" == "ON" ]] ; then \
    add-apt-repository "deb https://apt.repos.intel.com/oneapi all main" && \
    apt-get update && apt-get install -y intel-oneapi-compiler-dpcpp-cpp level-zero-dev; \
    fi

ARG ENABLE_PAHO_INSTALLATION=ON
ARG ENABLE_RDKAFKA_INSTALLATION=ON
RUN if [[ "${ENABLE_PAHO_INSTALLATION}" == "ON" ]] || [[ "${ENABLE_RDKAFKA_INSTALLATION}" == "ON" ]]; then \
    ./dlstreamer/scripts/install_metapublish_dependencies.sh; \
    fi

RUN /opt/intel/openvino_2022/install_dependencies/install_NEO_OCL_driver.sh -y

# Set environment
ENV ACL_BOARD_VENDOR_PATH=/opt/Intel/OpenCLFPGA/oneAPI/Boards
ENV CPATH=/opt/intel/oneapi/compiler/latest/linux/include:${CPATH}
ENV INTELFPGAOCLSDKROOT=/opt/intel/oneapi/compiler/latest/linux/lib/oclfpga
ENV LD_LIBRARY_PATH=/opt/intel/oneapi/compiler/latest/linux/lib:/opt/intel/oneapi/compiler/latest/linux/lib/x64:/opt/intel/oneapi/compiler/latest/linux/lib/emu:/opt/intel/oneapi/compiler/latest/linux/lib/oclfpga/host/linux64/lib:/opt/intel/oneapi/compiler/latest/linux/lib/oclfpga/linux64/lib:/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin:/opt/intel/oneapi/compiler/latest/linux/compiler/lib:${LD_LIBRARY_PATH}
ENV LIBRARY_PATH=/opt/intel/oneapi/compiler/latest/linux/compiler/lib/intel64_lin:/opt/intel/oneapi/compiler/latest/linux/lib:${LIBRARY_PATH}
ENV MANPATH=/opt/intel/oneapi/compiler/latest/documentation/en/man/common:${MANPATH}
ENV OCL_ICD_FILENAMES=libintelocl_emu.so:libalteracl.so:/opt/intel/oneapi/compiler/latest/linux/lib/x64/libintelocl.so
ENV PATH=/opt/intel/oneapi/compiler/latest/linux/lib/oclfpga/llvm/aocl-bin:/opt/intel/oneapi/compiler/latest/linux/lib/oclfpga/bin:/opt/intel/oneapi/compiler/latest/linux/bin/intel64:/opt/intel/oneapi/compiler/latest/linux/bin:/opt/intel/oneapi/compiler/latest/linux/ioc/bin:${PATH}

ENV DLSTREAMER_DIR="/opt/intel/dlstreamer"
ENV INTEL_OPENVINO_DIR="/opt/intel/openvino_2022"
ENV HDDL_INSTALL_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl"
ENV TBB_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/cmake"

ENV OpenCV_DIR="$INTEL_OPENVINO_DIR/extras/opencv/cmake"
ENV OpenVINO_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV InferenceEngine_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV ngraph_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"

ENV LD_LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/lib/gstreamer-1.0:${DLSTREAMER_DIR}/gstreamer/lib:${DLSTREAMER_DIR}/gstreamer/lib/gstreamer-1.0:$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/extras/opencv/lib/:$INTEL_OPENVINO_DIR/runtime/lib/intel64:/usr/lib:$LD_LIBRARY_PATH"
ENV PYTHONPATH="${DLSTREAMER_DIR}/python:${DLSTREAMER_DIR}/gstreamer/lib/python3.8/site-packages:$INTEL_OPENVINO_DIR/python/python3.8:$INTEL_OPENVINO_DIR/python/python3:$PYTHONPATH"

ENV GST_PLUGIN_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib/gstreamer-1.0:${GST_PLUGIN_PATH}"
ENV LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib:$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/extras/opencv/lib/:$INTEL_OPENVINO_DIR/runtime/lib/intel64:/usr/lib:${LIBRARY_PATH}"
ENV PKG_CONFIG_PATH="${DLSTREAMER_DIR}/lib/pkgconfig:${DLSTREAMER_DIR}/gstreamer/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
ENV MODELS_PATH="${MODELS_PATH:-${DLS_HOME}/intel/dl_streamer/models}"
ENV LC_NUMERIC="C"
ENV LIBVA_DRIVER_NAME="iHD"

ENV PATH="${DLSTREAMER_DIR}/gstreamer/bin:${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0:${PATH}"

ENV GI_TYPELIB_PATH="${DLSTREAMER_DIR}/gstreamer/lib/girepository-1.0"
ENV GST_PLUGIN_SCANNER="${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0/gst-plugin-scanner"

# Build DL Streamer
ARG BUILD_TYPE=Release
ARG EXTERNAL_DLS_BUILD_FLAGS
ARG GIT_INFO

RUN mkdir ./dlstreamer/build \
    && cd ./dlstreamer/build \
    && cmake \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${DLSTREAMER_DIR} \
    -DGIT_INFO=${GIT_INFO} \
    -DENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION} \
    -DENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION} \
    -DENABLE_VAAPI=ON \
    ${EXTERNAL_DLS_BUILD_FLAGS} \
    .. \
    && make -j $(nproc) \
    && make install \
    && ldconfig \
    && chown -R dlstreamer ${DLS_HOME}/dlstreamer

USER dlstreamer

WORKDIR ${DLSTREAMER_DIR}/samples
CMD ["/bin/bash"]
