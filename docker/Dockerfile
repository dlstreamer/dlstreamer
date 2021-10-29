# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG DOCKER_PRIVATE_REGISTRY

FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04 as ov-build
WORKDIR /root
USER root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

ENV HTTP_PROXY=${http_proxy}
ENV HTTPS_PROXY=${https_proxy}

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install -y -q --no-install-recommends cpio \
    pciutils \
    wget \
    software-properties-common \
    python3 \
    python3-pip \
    python3-dev \
    && add-apt-repository ppa:deadsnakes/ppa

ARG OPENVINO_URL
ARG OpenVINO_VERSION

ADD ${OPENVINO_URL} .

RUN tar -xzf *.tgz \
    && cd l_openvino_toolkit*p_${OpenVINO_VERSION} \
    && sed -i 's@rpm -Uvh https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm@rpm -Uvh https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm || true@g' ./install_openvino_dependencies.sh \
    && OpenVINO_YEAR="$(echo ${OpenVINO_VERSION} | cut -d "." -f 1)" \
    && sed -i 's/decline/accept/g' silent.cfg \
    && ./install.sh -s silent.cfg \
    && ln --symbolic /opt/intel/openvino_${OpenVINO_VERSION}/ /opt/intel/openvino \
    && cp ./rpm/intel-openvino-mediasdk* /opt/intel/mediasdk/

FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04 as gst-build
ENV HOME=/home
WORKDIR ${HOME}
USER root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

ENV HTTP_PROXY=${http_proxy}
ENV HTTPS_PROXY=${https_proxy}

# COMMON BUILD TOOLS
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
    cmake \
    build-essential \
    automake \
    autoconf \
    openssl \
    make \
    git \
    wget \
    gpg-agent \
    software-properties-common \
    pciutils \
    cpio \
    libtool \
    lsb-release \
    ca-certificates \
    pkg-config \
    bison \
    flex \
    libcurl4-gnutls-dev \
    zlib1g-dev \
    nasm \
    yasm \
    xorg-dev \
    libgl1-mesa-dev \
    openbox \
    python3 \
    python3-pip \
    python3-setuptools && \
    rm -rf /var/lib/apt/lists/*


ARG PACKAGE_ORIGIN="https://gstreamer.freedesktop.org"

ARG PREFIX=/
ARG LIBDIR=lib/
ARG LIBEXECDIR=bin/

ARG GST_VERSION=1.18.4
ARG BUILD_TYPE=release

ENV PATCHES_ROOT=${HOME}/build/src/patches
ENV SYS_PATCHES_DIR=${HOME}/src/patches
RUN mkdir -p ${PATCHES_ROOT} && mkdir -p ${SYS_PATCHES_DIR}

RUN pip3 install --no-cache-dir meson ninja

COPY --from=ov-build /opt/intel /opt/intel

# Build gstreamer plugin vaapi
ARG GST_PLUGIN_VAAPI_REPO=https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-${GST_VERSION}.tar.xz

RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install -y -q --no-install-recommends \
    libva-dev \
    libxrandr-dev \
    libudev-dev \
    libgtk-3-dev && \
    rm -rf /var/lib/apt/lists/*

# download gstreamer-vaapi
RUN wget ${GST_PLUGIN_VAAPI_REPO} -O build/src/gstreamer-vaapi-${GST_VERSION}.tar.xz
RUN tar xvf build/src/gstreamer-vaapi-${GST_VERSION}.tar.xz

# download gstreamer-vaapi patch
ARG GSTREAMER_VAAPI_PATCH_URL=https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435.patch
ARG GSTREAMER_VAAPI_PATCH_DMA_URL=https://raw.githubusercontent.com/openvinotoolkit/dlstreamer_gst/master/patches/gstreamer-vaapi/dma-vaapiencode.patch
RUN wget ${GSTREAMER_VAAPI_PATCH_URL} -O ${PATCHES_ROOT}/gst-vaapi-peek-vadisplay.patch
RUN wget ${GSTREAMER_VAAPI_PATCH_DMA_URL} -O ${PATCHES_ROOT}/dma-vaapiencode.patch

# put gstreamer-vaapi license along with the patch
RUN mkdir ${PATCHES_ROOT}/gstreamer_vaapi_patch_license && \
    cp gstreamer-vaapi-${GST_VERSION}/COPYING.LIB ${PATCHES_ROOT}/gstreamer_vaapi_patch_license/LICENSE

ARG MESON_GST_TESTS=disabled

RUN source /opt/intel/openvino/bin/setupvars.sh && \
    cd gstreamer-vaapi-${GST_VERSION} && \
    git apply ${PATCHES_ROOT}/gst-vaapi-peek-vadisplay.patch && \
    git apply ${PATCHES_ROOT}/dma-vaapiencode.patch && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dexamples=${MESON_GST_TESTS} \
    -Dtests=${MESON_GST_TESTS} \
    -Dgtk_doc=disabled \
    -Dnls=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    build/ && \
    ninja -C build

RUN rm /opt/intel/openvino/data_processing/gstreamer/lib/gstreamer-1.0/libgstvaapi.so && \
    cp /home/gstreamer-vaapi-${GST_VERSION}/build/gst/vaapi/libgstvaapi.so /opt/intel/openvino/data_processing/gstreamer/lib/gstreamer-1.0/ &&\
    chmod 644 /opt/intel/openvino/data_processing/gstreamer/lib/gstreamer-1.0/libgstvaapi.so


FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04

LABEL Description="This is the base image for GSTREAMER & OpenVINO™ Toolkit Ubuntu 20.04 LTS"
LABEL Vendor="Intel Corporation"
WORKDIR /root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

ENV HTTP_PROXY=${http_proxy}
ENV HTTPS_PROXY=${https_proxy}

# Prerequisites
RUN apt-get update && apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
    lsb-release python3-yaml python3-wheel python3-pip python3-setuptools python3-dev python-gi-dev sudo git wget curl pkg-config cmake clinfo vainfo gobject-introspection libusb-1.0.0 gnupg2 software-properties-common \
    opencl-headers ocl-icd-opencl-dev gpg-agent gcovr vim gdb ca-certificates uuid-dev libva-dev libva-drm2 ocl-icd-libopencl1

# Copy
COPY --from=gst-build /opt/intel /opt/intel

# Install NEO OCL drivers
RUN mkdir neo && cd neo \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-gmmlib_20.2.4_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-igc-core_1.0.4756_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-igc-opencl_1.0.4756_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-opencl_20.35.17767_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-ocloc_20.35.17767_amd64.deb \
    && dpkg -i intel*.deb


ENV XDG_RUNTIME_DIR=${PATH}:/usr/xdgr
ENV DISPLAY=:0.0

ARG GIT_INFO

# Install stable MediaSDK version
RUN add-apt-repository universe \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends alien \
    && alien -i /opt/intel/mediasdk/intel-openvino-mediasdk*.rpm


# Install DL Streamer
ARG OV_DLSTREAMER_DIR="/opt/intel/openvino/data_processing/dl_streamer"
ARG GST_GIT_URL="https://github.com/openvinotoolkit/dlstreamer_gst.git"

RUN git clone ${GST_GIT_URL} dl-streamer \
    && cd dl-streamer \
    && git submodule init \
    && git submodule update \
    && python3 -m pip install --no-cache-dir -r requirements.txt

ARG ENABLE_PAHO_INSTALLATION=ON
ARG ENABLE_RDKAFKA_INSTALLATION=ON
ARG BUILD_TYPE=Release
ARG EXTERNAL_GVA_BUILD_FLAGS

# Install Intel® oneAPI DPC++/C++ Compiler
ARG ENABLE_DPCPP_INSTALLATION
RUN if [ "$ENABLE_DPCPP_INSTALLATION" = "ON" ] ; then \
        cd dl-streamer/scripts && \
        ./install_dpcpp_dependencies.sh --type devel --upgrade_driver; \
    fi

ENV ACL_BOARD_VENDOR_PATH=/opt/Intel/OpenCLFPGA/oneAPI/Boards
ENV CPATH=/opt/intel/oneapi/compiler/2021.2.0/linux/include:${CPATH}
ENV INTELFPGAOCLSDKROOT=/opt/intel/oneapi/compiler/2021.2.0/linux/lib/oclfpga
ENV LD_LIBRARY_PATH=/opt/intel/oneapi/compiler/2021.2.0/linux/lib:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/x64:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/emu:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/oclfpga/host/linux64/lib:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/oclfpga/linux64/lib:/opt/intel/oneapi/compiler/2021.2.0/linux/compiler/lib/intel64_lin:/opt/intel/oneapi/compiler/2021.2.0/linux/compiler/lib:${LD_LIBRARY_PATH}
ENV LIBRARY_PATH=/opt/intel/oneapi/compiler/2021.2.0/linux/compiler/lib/intel64_lin:/opt/intel/oneapi/compiler/2021.2.0/linux/lib:${LIBRARY_PATH}
ENV MANPATH=/opt/intel/oneapi/compiler/2021.2.0/documentation/en/man/common:${MANPATH}
ENV OCL_ICD_FILENAMES=libintelocl_emu.so:libalteracl.so:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/x64/libintelocl.so
ENV PATH=/opt/intel/oneapi/compiler/2021.2.0/linux/lib/oclfpga/llvm/aocl-bin:/opt/intel/oneapi/compiler/2021.2.0/linux/lib/oclfpga/bin:/opt/intel/oneapi/compiler/2021.2.0/linux/bin/intel64:/opt/intel/oneapi/compiler/2021.2.0/linux/bin:/opt/intel/oneapi/compiler/2021.2.0/linux/ioc/bin:${PATH}


RUN cd /opt/intel/openvino/install_dependencies/ \
    && ./install_openvino_dependencies.sh -y

RUN source /opt/intel/openvino/bin/setupvars.sh \
    && mkdir -p dl-streamer/build \
    && cd dl-streamer/build \
    && cmake \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGIT_INFO=${GIT_INFO} \
    -DENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION} \
    -DENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION} \
    -DENABLE_VAAPI=ON \
    -DENABLE_VAS_TRACKER=ON \
    ${EXTERNAL_GVA_BUILD_FLAGS} \
    .. \
    && make -j $(nproc) \
    && make install \
    && ldconfig \
    && rm -rf ${OV_DLSTREAMER_DIR}/lib \
    && rm -rf ${OV_DLSTREAMER_DIR}/samples \
    && cp -r ../* ${OV_DLSTREAMER_DIR} \
    && ln --symbolic ${OV_DLSTREAMER_DIR}/build/intel64/Release/lib ${OV_DLSTREAMER_DIR}/lib \
    && rm -rf ../../dl-streamer


RUN printf "\nsource /opt/intel/openvino/bin/setupvars.sh\n" >> /root/.bashrc

# Install dependencies for download_models.sh script
RUN pip3 install -r ${OV_DLSTREAMER_DIR}/requirements.txt \
    && cd /opt/intel/openvino/deployment_tools/model_optimizer/install_prerequisites/ \
    && ./install_prerequisites.sh

WORKDIR ${OV_DLSTREAMER_DIR}/samples
CMD ["/bin/bash"]
