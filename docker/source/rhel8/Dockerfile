# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

ARG http_proxy
ARG https_proxy
ARG no_proxy
ARG ACTIVATED_IMAGE

FROM ${ACTIVATED_IMAGE} as gst-build
ENV HOME=/home
WORKDIR ${HOME}
USER root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

# Common build tools
RUN yum update -y && \
    yum install -y \
    cmake \
    automake \
    autoconf \
    openssl \
    make \
    xz \
    git \
    gcc \
    gcc-c++ \
    make \
    curl \
    flex \
    bison \
    pkgconfig \
    libtool \
    glib2-devel \
    redhat-lsb-core \
    ca-certificates \
    pkg-config \
    python3-pip \
    python3-setuptools && \
    yum clean all && \
    pip3 install --upgrade pip


ARG DLSTREAMER_INSTALL_DIR="/opt/intel/dlstreamer"
ARG GSTREAMER_INSTALL_DIR=${DLSTREAMER_INSTALL_DIR}/gstreamer
RUN mkdir -p ${DLSTREAMER_INSTALL_DIR}

ARG PREFIX=${GSTREAMER_INSTALL_DIR}
ARG LIBDIR=lib/
ARG LIBEXECDIR=bin/

ARG PACKAGE_ORIGIN="https://gstreamer.freedesktop.org"
ARG GST_VERSION=1.18.4
ARG BUILD_TYPE=release

ARG GSTREAMER_LIB_DIR=${PREFIX}/${LIBDIR}
ENV LIBRARY_PATH=${GSTREAMER_LIB_DIR}:${GSTREAMER_LIB_DIR}/gstreamer-1.0:${LIBRARY_PATH}
ENV LD_LIBRARY_PATH=${LIBRARY_PATH}
ENV PKG_CONFIG_PATH=${GSTREAMER_LIB_DIR}/pkgconfig
ENV PATH=${PREFIX}/${LIBEXECDIR}:${PATH}
ENV PATCHES_ROOT=${HOME}/build/src/patches
ENV SYS_PATCHES_DIR=${HOME}/src/patches
RUN mkdir -p ${PATCHES_ROOT} && mkdir -p ${SYS_PATCHES_DIR}

RUN python3 -m pip install --no-cache-dir meson ninja

# Install GStreamer
# GStreamer core
ARG GST_REPO=https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-${GST_VERSION}.tar.xz
RUN curl --location ${GST_REPO} --output build/src/gstreamer-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gstreamer-${GST_VERSION}.tar.xz

RUN yum install -y \
    gmp-devel \
    gsl-devel \
    gobject-introspection-devel \
    libcap-devel \
    libcap \
    gettext-devel && \
    yum clean all

RUN cd gstreamer-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dbenchmarks=disabled \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

# ORC Acceleration
ARG GST_ORC_VERSION=0.4.31
ARG GST_ORC_REPO=https://gstreamer.freedesktop.org/src/orc/orc-${GST_ORC_VERSION}.tar.xz
RUN curl --location ${GST_ORC_REPO} --output build/src/orc-${GST_ORC_VERSION}.tar.xz && \
    tar xvf build/src/orc-${GST_ORC_VERSION}.tar.xz

RUN cd orc-${GST_ORC_VERSION} && \
    meson \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dbenchmarks=disabled \
    -Dorc-test=disabled \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

RUN yum install -y \
    libXrandr-devel \
    libX11-devel \
    iso-codes-devel \
    mesa-libEGL-devel \
    mesa-libGLES-devel \
    mesa-libGL-devel \
    libgudev1 \
    libtheora-devel \
    cdparanoia-devel \
    pango-devel \
    mesa-libgbm-devel \
    alsa-lib-devel \
    libjpeg-turbo-devel \
    libvisual-devel \
    libXv-devel \
    opus-devel && \
    yum clean all

# Build vorbis
ARG VORBIS_URL=https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz
RUN mkdir -p build/src/vorbis && \
    curl --location ${VORBIS_URL} --output build/src/vorbis/libvorbis-1.3.7.tar.xz && \
    tar xvf build/src/vorbis/libvorbis-1.3.7.tar.xz

RUN cd libvorbis-1.3.7 && \
    ./autogen.sh && \
    ./configure && \
    make && \
    make install

# Build the GStreamer Plugin Base
ARG GST_PLUGIN_BASE_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-base/gst-plugins-base-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PLUGIN_BASE_REPO} --output build/src/gst-plugins-base-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gst-plugins-base-${GST_VERSION}.tar.xz

RUN cd gst-plugins-base-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dgl=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dc_std=gnu11 \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/


# GStreamer Good plugins
RUN yum install -y \
    patch \
    bzip2-devel \
    libv4l-devel \
    flac-devel \
    gdk-pixbuf2-devel \
    libdv-devel \
    mpg123-devel \
    libraw1394-devel \
    libiec61883-devel \
    pulseaudio-libs-devel \
    libsoup-devel \
    speex-devel \
    wavpack-devel && \
    yum clean all

ARG GST_PLUGIN_GOOD_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PLUGIN_GOOD_REPO} --output build/src/gst-plugins-good-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gst-plugins-good-${GST_VERSION}.tar.xz

RUN cd gst-plugins-good-${GST_VERSION}  && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

# GStreamer Bad plugins
RUN rpm --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro

RUN yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm && \
    subscription-manager repos --enable "codeready-builder-for-rhel-8-$(arch)-rpms"

RUN rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm && \
    rpm -Uvh https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm

RUN yum install -y \
    bluez-libs-devel \
    libusb-devel \
    libass-devel \
    libbs2b-devel \
    libchromaprint-devel \
    lcms2-devel \
    libdc1394-devel \
    libXext-devel \
    libssh-devel \
    libdca-devel \
    faac-devel \
    fdk-aac-devel \
    flite-devel \
    fluidsynth-devel \
    game-music-emu-devel \
    gsm-devel \
    nettle-devel \
    liblrdf-devel \
    mjpegtools-devel \
    libmodplug-devel \
    libmpcdec-devel \
    neon-devel \
    openal-soft-devel \
    OpenEXR-devel \
    openjpeg2-devel \
    librsvg2-devel \
    libsndfile-devel \
    soundtouch-devel \
    spandsp-devel \
    libsrtp-devel \
    zvbi-devel \
    wildmidi-devel \
    libnice-devel \
    libxkbcommon-devel && \
    yum clean all

ARG GST_PLUGIN_BAD_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-bad/gst-plugins-bad-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PLUGIN_BAD_REPO} --output build/src/gst-plugins-bad-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gst-plugins-bad-${GST_VERSION}.tar.xz

RUN mkdir ${PATCHES_ROOT}/gst_plugins_bad_patch_license && \
    cp gst-plugins-bad-${GST_VERSION}/COPYING ${PATCHES_ROOT}/gst_plugins_bad_patch_license/LICENSE

RUN cd gst-plugins-bad-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Ddoc=disabled \
    -Dnls=disabled \
    -Dx265=disabled \
    -Dresindvd=disabled \
    -Dmplex=disabled \
    -Ddts=disabled \
    -Dofa=disabled \
    -Dfaad=disabled \
    -Dmpeg2enc=disabled \
    -Dvoamrwbenc=disabled \
    -Drtmp=disabled \
    -Dwebrtcdsp=disabled \
    -Dlibmms=disabled \
    -Dlibde265=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dc_std=gnu11 \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/


# Build the GStreamer Plugin Ugly set
ARG GST_PLUGIN_UGLY_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-ugly/gst-plugins-ugly-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PLUGIN_UGLY_REPO} --output build/src/gst-plugins-ugly-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gst-plugins-ugly-${GST_VERSION}.tar.xz

RUN yum install -y libmpeg2-devel && yum clean all

RUN cd gst-plugins-ugly-${GST_VERSION}  && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dcdio=disabled \
    -Dmpeg2dec=disabled \
    -Ddvdread=disabled \
    -Da52dec=disabled \
    -Dx264=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

# FFmpeg
RUN yum install -y bzip2
RUN mkdir ffmpeg_sources && cd ffmpeg_sources && \
    curl --output - --location https://www.nasm.us/pub/nasm/releasebuilds/2.14.02/nasm-2.14.02.tar.bz2 | tar xj && \
    cd nasm-2.14.02 && \
    ./autogen.sh && \
    ./configure --prefix=${PREFIX} --bindir="${PREFIX}/bin" && \
    make && make install

# Download patch
RUN git clone -n https://github.com/FFmpeg/FFmpeg.git && \
    cd FFmpeg && \
    git format-patch -1 26d3c81bc5ef2f8c3f09d45eaeacfb4b1139a777 --stdout > ${PATCHES_ROOT}/ffmpeg_check_dc_count.patch

RUN curl --location https://ffmpeg.org/releases/ffmpeg-4.4.tar.gz --output build/src/ffmpeg-4.4.tar.gz
RUN cd ffmpeg_sources && \
    tar xvf /home/build/src/ffmpeg-4.4.tar.gz && \
    cd ffmpeg-4.4 && \
    cat ${PATCHES_ROOT}/ffmpeg_check_dc_count.patch | git apply && \
    PATH="${PREFIX}/bin:$PATH" PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig" \
    ./configure \
    --disable-gpl \
    --enable-pic \
    --disable-shared \
    --enable-static \
    --prefix=${PREFIX} \
    --extra-cflags="-I${PREFIX}/include" \
    --extra-ldflags="-L${PREFIX}/lib" \
    --extra-libs=-lpthread \
    --extra-libs=-lm \
    --bindir="${PREFIX}/bin" \
    --disable-vaapi && \
    make -j $(nproc) && \
    make install

# Download gstreamer-libav patch, can be removed with 1.19 transition
ARG GSTREAMER_LIBAV_PATCH_URL=https://github.com/GStreamer/gst-libav/commit/75fb364bf435d80a51f1ecba6afc999b5f36292e.patch
RUN curl --location ${GSTREAMER_LIBAV_PATCH_URL} --output ${PATCHES_ROOT}/gst-libav-fix-performance.patch

# Build gst-libav
ARG GST_PLUGIN_LIBAV_REPO=https://gstreamer.freedesktop.org/src/gst-libav/gst-libav-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PLUGIN_LIBAV_REPO} --output build/src/gst-libav-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gst-libav-${GST_VERSION}.tar.xz

RUN cd gst-libav-${GST_VERSION} && \
    cat ${PATCHES_ROOT}/gst-libav-fix-performance.patch | git apply && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/


# Build GStreamer VAAPI plugin
ARG GST_PLUGIN_VAAPI_REPO=https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-${GST_VERSION}.tar.xz

RUN yum install -y \
    libva-devel \
    systemd-devel \
    libpciaccess-devel

# Install libdrm-2.4.98
RUN git clone -b libdrm-2.4.98 https://gitlab.freedesktop.org/mesa/drm.git && \
    cd drm && meson --prefix=/usr --libdir=/usr/lib64 build/ && \
    ninja -C build/ install

# Download gstreamer-vaapi & patches
RUN curl --location ${GST_PLUGIN_VAAPI_REPO} --output build/src/gstreamer-vaapi-${GST_VERSION}.tar.xz && \
    tar xvf build/src/gstreamer-vaapi-${GST_VERSION}.tar.xz

ARG GSTREAMER_VAAPI_PATCH_URL=https://gitlab.freedesktop.org/gstreamer/gstreamer-vaapi/-/merge_requests/435.patch
ARG GSTREAMER_VAAPI_PATCH_DMA_URL=https://raw.githubusercontent.com/dlstreamer/dlstreamer/master/patches/gstreamer-vaapi/dma-vaapiencode.patch
RUN curl --location ${GSTREAMER_VAAPI_PATCH_URL} --output ${PATCHES_ROOT}/gst-vaapi-peek-vadisplay.patch
RUN curl --location ${GSTREAMER_VAAPI_PATCH_DMA_URL} --output ${PATCHES_ROOT}/dma-vaapiencode.patch

# Put gstreamer-vaapi license along with the patch
RUN mkdir ${PATCHES_ROOT}/gstreamer_vaapi_patch_license && \
    cp gstreamer-vaapi-${GST_VERSION}/COPYING.LIB ${PATCHES_ROOT}/gstreamer_vaapi_patch_license/LICENSE
RUN mkdir ${PATCHES_ROOT}/gstreamer_vaapi_dma_vaapiencode_patch_license && \
    cp gstreamer-vaapi-${GST_VERSION}/COPYING.LIB ${PATCHES_ROOT}/gstreamer_vaapi_dma_vaapiencode_patch_license/LICENSE

RUN cd gstreamer-vaapi-${GST_VERSION} && \
    git apply ${PATCHES_ROOT}/gst-vaapi-peek-vadisplay.patch && \
    git apply ${PATCHES_ROOT}/dma-vaapiencode.patch && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    ninja install -C build/

# gst-python
RUN yum install -y \
    pygobject3-devel \
    python3-devel && \
    yum clean all

ARG GST_PYTHON_REPO=https://gstreamer.freedesktop.org/src/gst-python/gst-python-${GST_VERSION}.tar.xz
RUN curl --location ${GST_PYTHON_REPO} --output build/src/gst-python-${GST_VERSION}.tar.xz
RUN tar xvf build/src/gst-python-${GST_VERSION}.tar.xz && \
    cd gst-python-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dpython=python3 \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

ENV GI_TYPELIB_PATH=${GSTREAMER_LIB_DIR}/girepository-1.0
ENV PYTHONPATH=${PREFIX}/lib/python3.6/site-packages:${PYTHONPATH}

ARG GST_RTSP_SERVER_REPO=https://gstreamer.freedesktop.org/src/gst-rtsp-server/gst-rtsp-server-${GST_VERSION}.tar.xz
RUN curl --location ${GST_RTSP_SERVER_REPO} --output build/src/gst-rtsp-server-${GST_VERSION}.tar.xz
RUN tar xf build/src/gst-rtsp-server-${GST_VERSION}.tar.xz && \
    cd gst-rtsp-server-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/


# Building DL Streamer image
FROM ${ACTIVATED_IMAGE} as dlstreamer-build

LABEL Description="This is the development image of Intel® Deep Learning Streamer Pipeline Framework (Intel® DL Streamer Pipeline Framework) for Red Hat Enterprise Linux 8"
LABEL Vendor="Intel Corporation"

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

WORKDIR /root
USER root

ENV http_proxy=${http_proxy}
ENV https_proxy=${https_proxy}
ENV no_proxy=${no_proxy}
ENV HTTP_PROXY=${http_proxy}
ENV HTTPS_PROXY=${https_proxy}
ENV NO_PROXY=${no_proxy}

ARG DLSTREAMER_INSTALL_DIR="/opt/intel/dlstreamer"

COPY --from=gst-build /opt/intel /opt/intel

ARG LIBDIR=lib/
ARG LIBEXECDIR=bin/
ARG GSTREAMER_INSTALL_DIR=${DLSTREAMER_INSTALL_DIR}/gstreamer
ARG GSTREAMER_LIB_DIR=${GSTREAMER_INSTALL_DIR}/${LIBDIR}
ENV LIBRARY_PATH=${GSTREAMER_LIB_DIR}:${GSTREAMER_LIB_DIR}/gstreamer-1.0:${LIBRARY_PATH}
ENV LD_LIBRARY_PATH=${LIBRARY_PATH}:${LD_LIBRARY_PATH}
ENV PKG_CONFIG_PATH=${GSTREAMER_LIB_DIR}/pkgconfig:${PKG_CONFIG_PATH}
ENV PATH=${GSTREAMER_INSTALL_DIR}/${LIBEXECDIR}:${PATH}

# Prerequisites
RUN yum update -y --nobest && \
    yum upgrade -y --nobest && \
    yum install -y https://download-ib01.fedoraproject.org/pub/epel/7/x86_64/Packages/c/clinfo-2.1.17.02.09-1.el7.x86_64.rpm \
    redhat-lsb-core \
    python3-yaml \
    python3-wheel \
    gcc \
    gcc-c++ \
    flex \
    bison \
    make \
    curl \
    pkgconfig \
    libtool \
    glib2-devel \
    sudo \
    cmake \
    git \
    gobject-introspection \
    libusb-devel \
    gnupg2 \
    vim \
    gdb \
    opencl-headers


# Register Intel® Graphics repository
ARG GRAPHICS_APT_REPO=https://repositories.intel.com/graphics
RUN RHEL_VERSION=`. /etc/os-release; echo ${VERSION_ID}` && \
    dnf install -y 'dnf-command(config-manager)' && \
    dnf config-manager --add-repo \
    ${GRAPHICS_APT_REPO}/rhel/${RHEL_VERSION}/intel-graphics.repo

RUN dnf install -y \
    intel-opencl intel-media intel-mediasdk \
    level-zero intel-level-zero-gpu \
    intel-igc-opencl-devel \
    intel-igc-cm level-zero-devel

ENV XDG_RUNTIME_DIR=${PATH}:/usr/xdgr
ENV DISPLAY=:0.0


# Install Intel® OpenVINO
ARG OPENVINO_REPO_URL=https://yum.repos.intel.com/openvino/2022
RUN echo \
$'[OpenVINO] \n\
name=Intel(R) Distribution of OpenVINO 2022 \n\
baseurl='${OPENVINO_REPO_URL}$' \n\
enabled=1 \n\
gpgcheck=1 \n\
repo_gpgcheck=1 \n\
gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB ' > /etc/yum.repos.d/openvino-2022.repo

RUN yum list -y openvino*
RUN yum install -y openvino openvino-opencv
RUN /opt/intel/openvino_2022/install_dependencies/install_openvino_dependencies.sh -y

ARG EXTRA_PYPI_INDEX_URL
RUN yum install -y python3-pip && pip3 install --upgrade pip
RUN if [ -n "$EXTRA_PYPI_INDEX_URL"  ] ; then \
    python3 -m pip config set global.extra-index-url ${EXTRA_PYPI_INDEX_URL} ;\
    fi
RUN python3 -m pip install openvino-dev[onnx,tensorflow2,pytorch,mxnet] --ignore-installed PyYAML

RUN useradd -ms /bin/bash -u 1000 -G video dlstreamer
ARG DLS_HOME=/home/dlstreamer
WORKDIR ${DLS_HOME}

# Download DL Streamer source code
ARG DLSTREAMER_GIT_URL="https://github.com/dlstreamer/dlstreamer.git"
ARG DLSTREAMER_SOURCE_DIR=${DLS_HOME}/dlstreamer
RUN git clone ${DLSTREAMER_GIT_URL} ${DLSTREAMER_SOURCE_DIR} \
    && cd ${DLSTREAMER_SOURCE_DIR} \
    && git submodule update --init \
    && python3 -m pip install --no-cache-dir -r requirements.txt

ARG ENABLE_PAHO_INSTALLATION=ON
ARG ENABLE_RDKAFKA_INSTALLATION=ON
ARG BUILD_TYPE=Release
ARG EXTERNAL_DLS_BUILD_FLAGS

# Install Metapublish dependencies
ARG ENABLE_PAHO_INSTALLATION=ON
ARG ENABLE_RDKAFKA_INSTALLATION=ON
RUN if [[ "$ENABLE_PAHO_INSTALLATION" = "ON" || "$ENABLE_RDKAFKA_INSTALLATION" = "ON" ]] ; then \
    ${DLSTREAMER_SOURCE_DIR}/scripts/install_metapublish_dependencies.sh; \
    fi

RUN rpm --import http://li.nux.ro/download/nux/RPM-GPG-KEY-nux.ro
RUN yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm && \
    subscription-manager repos --enable "codeready-builder-for-rhel-8-$(arch)-rpms"
RUN rpm -Uvh http://li.nux.ro/download/nux/dextop/el7/x86_64/nux-dextop-release-0-5.el7.nux.noarch.rpm && \
    rpm -Uvh https://download1.rpmfusion.org/free/el/rpmfusion-free-release-7.noarch.rpm
RUN yum install -y \
    cpio pciutils automake autoconf openssl make  xz pkg-config \
    libva-devel systemd-devel ca-certificates \
    python3 python3-devel python3-setuptools pygobject3-devel \
    gmp gsl libcap gettext libXrandr libX11 iso-codes libXv \
    mesa-libEGL mesa-libGLES mesa-libGL \
    libgudev1 libtheora cdparanoia \
    pango mesa-libgbm alsa-lib libjpeg-turbo \
    libvisual bzip2 libv4l flac gdk-pixbuf2 libdv mpg123 \
    libraw1394 libiec61883 pulseaudio-libs libsoup \
    speex wavpack opus bluez-libs \
    libass-devel libbs2b libchromaprint lcms2 \
    libdc1394 libXext libssh libdca faac \
    fdk-aac flite fluidsynth game-music-emu gsm \
    nettle liblrdf libmodplug libmpcdec neon \
    openal-soft OpenEXR-devel openjpeg2 librsvg2 \
    libsndfile soundtouch spandsp libsrtp zvbi \
    wildmidi libnice libxkbcommon numactl && \
    yum clean all

# Build DL Streamer
ARG GIT_INFO
RUN source /opt/intel/openvino_2022/setupvars.sh && \
    mkdir -p ${DLSTREAMER_SOURCE_DIR}/build && \
    cd ${DLSTREAMER_SOURCE_DIR}/build  && \
    cmake \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DCMAKE_INSTALL_PREFIX=${DLSTREAMER_INSTALL_DIR} \
    -DGIT_INFO=${GIT_INFO} \
    -DENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION} \
    -DENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION} \
    -DENABLE_VAAPI=ON \
    ${EXTERNAL_DLS_BUILD_FLAGS} \
    .. && \
    make -j $(nproc) && \
    make install

RUN chown -R dlstreamer ${DLS_HOME}/dlstreamer
RUN chown -R dlstreamer ${DLSTREAMER_INSTALL_DIR}

# Setup environment
ENV DLSTREAMER_DIR=${DLSTREAMER_INSTALL_DIR}
ENV INTEL_OPENVINO_DIR="/opt/intel/openvino_2022"
ENV HDDL_INSTALL_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl"
ENV TBB_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/cmake"
ENV OpenCV_DIR="$INTEL_OPENVINO_DIR/extras/opencv/cmake/"
ENV OpenVINO_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV InferenceEngine_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV ngraph_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"

ENV PATH="${DLSTREAMER_DIR}/gstreamer/bin:${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0:${PATH}"
ENV LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib:$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/extras/opencv/lib/:$INTEL_OPENVINO_DIR/runtime/lib/intel64:/usr/lib:/usr/local/lib:${LIBRARY_PATH}"
ENV LD_LIBRARY_PATH="${DLSTREAMER_DIR}/lib/gstreamer-1.0:${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib:$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/extras/opencv/lib/:$INTEL_OPENVINO_DIR/runtime/lib/intel64:/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"
ENV PKG_CONFIG_PATH="${DLSTREAMER_DIR}/lib/pkgconfig:${DLSTREAMER_DIR}/gstreamer/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
ENV PYTHONPATH="${DLSTREAMER_DIR}/python:${DLSTREAMER_DIR}/gstreamer/lib/python3.8/site-packages:$INTEL_OPENVINO_DIR/python/python3.8:$INTEL_OPENVINO_DIR/python/python3:$PYTHONPATH"

ENV GST_PLUGIN_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/gstreamer/lib/gstreamer-1.0:${GST_PLUGIN_PATH}"
ENV GST_PLUGIN_SCANNER="${DLSTREAMER_DIR}/gstreamer/bin/gstreamer-1.0/gst-plugin-scanner"
ENV GI_TYPELIB_PATH="${DLSTREAMER_DIR}/gstreamer/lib/girepository-1.0:${GI_TYPELIB_PATH}"
ENV MODELS_PATH="${MODELS_PATH:-${DLS_HOME}/intel/dl_streamer/models}"

ENV LC_NUMERIC="C"
ENV LIBVA_DRIVER_NAME="iHD"

WORKDIR ${DLSTREAMER_DIR}/samples
USER dlstreamer
CMD ["/bin/bash"]
