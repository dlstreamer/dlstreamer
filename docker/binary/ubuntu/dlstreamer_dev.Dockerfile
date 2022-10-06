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

LABEL Description="This is the development image of Intel® Deep Learning Streamer Pipeline Framework (Intel® DL Streamer Pipeline Framework) for Ubuntu 20.04 LTS"
LABEL Vendor="Intel Corporation"

ARG DLSTREAMER_APT_VERSION="*"
ARG INSTALL_DPCPP=true

WORKDIR /root
USER root
SHELL ["/bin/bash", "-xo", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive

# Intel® Graphics APT repository
RUN cp /home/dlstreamer/intel-graphics.list /etc/apt/sources.list.d/

# Install Intel® Deep Learning Streamer (Intel® DL Streamer) development package
RUN apt-get update && apt-get install -y intel-dlstreamer-dev=${DLSTREAMER_APT_VERSION}

# Install OpenVINO™ samples and development tools
ARG OPENVINO_INSTALL_OPTIONS=
RUN ${DLSTREAMER_DIR}/install_dependencies/install_openvino.sh --include-samples --install-dev-tools ${OPENVINO_INSTALL_OPTIONS}

# If INSTALL_DPCPP set to true, install DPC++ compiler dev package
ARG DPCPP_APT_VERSION=*
RUN if [ "${INSTALL_DPCPP}" = "true" ] ; then \
    apt-get update && apt-get install -y intel-oneapi-compiler-dpcpp-cpp=${DPCPP_APT_VERSION} ; \
    apt-get update && apt-get install -y level-zero-dev ; \
    fi

# Install python modules
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python3-pip && pip3 install --upgrade pip
ARG EXTRA_PYPI_INDEX_URL
RUN if [ -n "$EXTRA_PYPI_INDEX_URL" ] ; then \
    python3 -m pip config set global.extra-index-url ${EXTRA_PYPI_INDEX_URL} ;\
    fi
RUN python3 -m pip install openvino-dev[onnx,tensorflow2,pytorch,mxnet]
RUN python3 -m pip install numpy opencv-python
RUN if [ -n "$EXTRA_PYPI_INDEX_URL" ] ; then \
    python3 -m pip config unset global.extra-index-url ;\
    fi

# Install build essential
RUN apt-get update && apt-get install -y cmake build-essential

# Install VAAPI and OpenCL info tools
RUN apt-get update && apt-get install -y vainfo clinfo

# Install packages with GStreamer debug symbols
RUN apt-get install -y ubuntu-dbgsym-keyring
RUN echo "deb http://ddebs.ubuntu.com $(lsb_release -cs) main restricted universe multiverse" | \
    tee -a /etc/apt/sources.list.d/ddebs.list
RUN apt-get update && apt-get install libgstreamer1.0-0-dbgsym \
    gstreamer1.0-plugins-base-dbgsym gstreamer1.0-plugins-good-dbgsym gstreamer1.0-plugins-bad-dbgsym \
    gstreamer1.0-plugins-ugly-dbgsym gstreamer1.0-libav-dbgsym gstreamer1.0-vaapi-dbgsym \
    || echo "dbgsym packages failed to install"

# Remove Intel® Graphics APT repository
RUN rm -rf /etc/apt/sources.list.d/intel-graphics.list
RUN apt-get update

WORKDIR /home/dlstreamer
USER dlstreamer
ENV DLSTREAMER_DIR=/opt/intel/dlstreamer
COPY ./docker/third-party-programs.txt ${DLSTREAMER_DIR}/

CMD ["/bin/bash"]
