# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG BASE_IMAGE

FROM ${BASE_IMAGE}

LABEL Description="This is the development image of Intel® Deep Learning Streamer Pipeline Framework (Intel® DL Streamer Pipeline Framework) for Ubuntu 18.04 LTS"
LABEL Vendor="Intel Corporation"

ARG DLSTREAMER_APT_VERSION="*"
ARG INSTALL_DPCPP=false

WORKDIR /root
USER root
SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

# Install Intel® Deep Learning Streamer (Intel® DL Streamer) development package
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y intel-dlstreamer-dev=${DLSTREAMER_APT_VERSION}

# If INSTALL_DPCPP set to true, install DPC++ compiler dev package
ARG DPCPP_APT_VERSION=*
RUN if [ "${INSTALL_DPCPP}" = "true" ] ; then \
    DEBIAN_FRONTEND=noninteractive apt-get install -y intel-oneapi-compiler-dpcpp-cpp=${DPCPP_APT_VERSION} ; \
    fi

# Install python modules
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python3-pip && pip3 install --upgrade pip
ARG EXTRA_PYPI_INDEX_URL
RUN if [ -n "$EXTRA_PYPI_INDEX_URL" ] ; then \
    python3 -m pip config set global.extra-index-url ${EXTRA_PYPI_INDEX_URL} ;\
    fi
RUN python3 -m pip install openvino-dev[onnx,tensorflow2,pytorch,mxnet]
RUN if [ -n "$EXTRA_PYPI_INDEX_URL" ] ; then \
    python3 -m pip config unset global.extra-index-url ;\
    fi

# Install build dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y cmake build-essential

WORKDIR /home/dlstreamer
USER dlstreamer
ENV DLSTREAMER_DIR=/opt/intel/dlstreamer
COPY ./docker/third-party-programs.txt ${DLSTREAMER_DIR}/

CMD ["/bin/bash"]
