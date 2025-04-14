# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

LABEL description="This is the development image of Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework"
LABEL vendor="Intel Corporation"

ARG DLSTREAMER_VERSION=2025.0.1.3
ARG GST_VERSION=1.24.12
ARG VORBIS_VERSION=1.3.7
ARG FFMPEG_VERSION=6.1.1

ARG PACKAGE_ORIGIN="https://gstreamer.freedesktop.org"
ARG GST_REPO="${PACKAGE_ORIGIN}"/src
ARG VORBIS_URL=https://downloads.xiph.org/releases/vorbis/libvorbis-${VORBIS_VERSION}.tar.xz
ARG OPENVINO_VERSION=2025.0
ARG OPENVINO_FILENAME=openvino_toolkit_ubuntu22_2025.0.0.17942.1f68be9f594_x86_64

ENV INTEL_DLSTREAMER_DIR=/opt/intel/dlstreamer
ENV GSTREAMER_SRC_DIR=/opt/intel/dlstreamer/gstreamer/src
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
ENV DLSTREAMER_DIR=/opt/intel/dlstreamer
ENV OpenVINO_DIR=/opt/intel/openvino_"$OPENVINO_VERSION".0/runtime/cmake
ENV SPDLOG_COMMIT=5ebfc927306fd7ce551fa22244be801cf2b9fdd9
ENV GOOGLETEST_COMMIT=f8d7d77c06936315286eb55f8de22cd23c188571
ENV LIBVA_DRIVER_NAME=iHD
ENV GST_VA_ALL_DRIVERS=1

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

RUN \
    apt-get update && \
    apt-get install -y -q --no-install-recommends wget=\* xz-utils=\* python3-pip=\* python3-gi=\* gcc-multilib=\* libglib2.0-dev=\* \
    flex=\* bison=\* autoconf=\* automake=\* libtool=\* libogg-dev=\* make=\* g++=\* libva-dev=\* yasm=\* libglx-dev=\* libdrm-dev=\* \
    python-gi-dev=\* python3-dev=\* libtbb12=\* gpg=\* unzip=\* libopencv-dev=\* libgflags-dev=\* \
    libgirepository1.0-dev=\* libx265-dev=\* libx264-dev=\* libde265-dev=\* gudev-1.0=\* libusb-1.0=\* nasm=\* python3-venv=\* \
    libcairo2-dev=\* libxt-dev=\* libgirepository1.0-dev=\* libgles2-mesa-dev=\* wayland-protocols=\* libcurl4-openssl-dev=\* \
    libssh2-1-dev=\* cmake=\* git=\* valgrind=\* numactl=\* libvpx-dev=\* libopus-dev=\* libsrtp2-dev=\* libxv-dev=\* \
    linux-libc-dev=\* libpmix2=\* libhwloc15=\* libhwloc-plugins=\* && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN \
    wget -q --no-check-certificate https://github.com/pkgconf/pkgconf/archive/refs/tags/pkgconf-2.3.0.zip && \
    unzip pkgconf-2.3.0.zip && \
    rm pkgconf-2.3.0.zip

WORKDIR /pkgconf-pkgconf-2.3.0

RUN \
    autoreconf -i && \
    ./configure && \
    make -j "$(nproc)" && \
    make install

WORKDIR /

RUN \
    useradd -ms /bin/bash dlstreamer && \
    usermod -a -G sudo dlstreamer && \
    mkdir /python3venv && \
    chown dlstreamer: /python3venv && \
    chmod u+w /python3venv

USER dlstreamer

RUN \
    python3 -m venv /python3venv && \
    /python3venv/bin/pip3 install --no-cache-dir --upgrade pip==24.0 && \
    /python3venv/bin/pip3 install --no-cache-dir --no-dependencies \
    meson==1.4.1 \
    ninja==1.11.1.1 \
    numpy==1.26.4 \
    tabulate==0.9.0 \
    tqdm==4.66.4 \
    junit-xml==1.9 \
    opencv-python==4.10.0.82 \
    XlsxWriter==3.2.0 \
    zxing-cpp==2.2.0 \
    pyzbar==0.1.9 \
    six==1.16.0 \
    pycairo==1.26.0 \
    PyGObject==3.50.0 \
    setuptools==70.0.0 \
    pytest==8.3.3 \
    pluggy==1.5.0 \
    exceptiongroup==1.2.2 \
    iniconfig==2.0.0

USER root

ENV PATH="/python3venv/bin:${PATH}"

# Intel® NPU drivers (optional)
RUN \
    mkdir debs && \
    dpkg --purge --force-remove-reinstreq intel-driver-compiler-npu intel-fw-npu intel-level-zero-npu level-zero && \
    wget -q https://github.com/oneapi-src/level-zero/releases/download/v1.17.44/level-zero_1.17.44+u22.04_amd64.deb -P ./debs && \
    wget -q --no-check-certificate -nH --accept-regex="ubuntu22.*\.deb" --cut-dirs=5 -r https://github.com/intel/linux-npu-driver/releases/expanded_assets/v1.13.0 -P ./debs && \      
    apt-get install -y -q --no-install-recommends ./debs/*.deb && \
    rm -r -f debs && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* && \
    rm -f /etc/ssl/certs/Intel*

# Intel® Data Center GPU Flex Series drivers (optional)
# hadolint ignore=SC1091
RUN \
    apt-get update && \
    . /etc/os-release && \
    if [[ ! "jammy" =~ ${VERSION_CODENAME} ]]; then \
        echo "Ubuntu version ${VERSION_CODENAME} not supported"; \
    else \
        wget --no-check-certificate -qO- https://repositories.intel.com/gpu/intel-graphics.key | gpg --dearmor --output /usr/share/keyrings/gpu-intel-graphics.gpg && \
        echo "deb [arch=amd64 signed-by=/usr/share/keyrings/gpu-intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu jammy client" | \
        tee /etc/apt/sources.list.d/intel-gpu-"${VERSION_CODENAME}".list && \
        apt-get update; \
    fi && \
    apt-get install -y --no-install-recommends \
    intel-opencl-icd=\* ocl-icd-opencl-dev=\* intel-level-zero-gpu=\* level-zero=\* \
    libmfx1=\* libmfxgen1=\* libvpl2=\* intel-media-va-driver-non-free=\* \
    libgbm1=\* libigdgmm12=\* libxatracker2=\* libdrm-amdgpu1=\* \
    va-driver-all=\* vainfo=\* hwinfo=\* clinfo=\* && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# GStreamer core
RUN \
    mkdir -p "$GSTREAMER_SRC_DIR" && \
    wget --quiet --no-check-certificate ${GST_REPO}/gstreamer/gstreamer-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gstreamer-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gstreamer-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gstreamer-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gstreamer-${GST_VERSION}

RUN \
    meson setup \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dbenchmarks=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

#Build vorbis
RUN \
    mkdir -p /src/vorbis && \
    wget -q --no-check-certificate ${VORBIS_URL} -O /src/vorbis/libvorbis-${VORBIS_VERSION}.tar.xz && \
    tar -xf /src/vorbis/libvorbis-${VORBIS_VERSION}.tar.xz -C /src/vorbis && \
    rm /src/vorbis/libvorbis-${VORBIS_VERSION}.tar.xz

WORKDIR /src/vorbis/libvorbis-${VORBIS_VERSION}

RUN \
    ./autogen.sh && \
    ./configure --disable-dependency-tracking && \
    make -j "$(nproc)" && \
    make install

# Build the gstreamer plugin base
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-plugins-base/gst-plugins-base-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-plugins-base-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-plugins-base-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-plugins-base-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-plugins-base-${GST_VERSION}

RUN \
    meson setup \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dgl=disabled \
    -Dxvideo=enabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build the gstreamer good plugins
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-plugins-good/gst-plugins-good-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-plugins-good-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-plugins-good-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-plugins-good-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-plugins-good-${GST_VERSION}

RUN \
    meson setup \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dlibcaca=disabled \
    -Dvpx=enabled \
    -Drtp=enabled \
    -Drtpmanager=enabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build openh264
RUN \
    wget --quiet --no-check-certificate https://github.com/cisco/openh264/archive/refs/tags/v2.4.1.zip -O /v2.4.1.zip && \
    unzip /v2.4.1.zip -d /

WORKDIR /openh264-2.4.1

RUN \
    make OS=linux ARCH=x86_64 -j "$(nproc)" && \
    make install && \
    rm ../v2.4.1.zip

# libnice
WORKDIR /

RUN \
    wget -q --no-check-certificate https://github.com/libnice/libnice/archive/refs/heads/master.zip -O libnice.zip && \
    unzip libnice.zip && \
    rm libnice.zip

WORKDIR /libnice-master

RUN \
    meson setup \
    -Dgstreamer=enabled \
    --prefix=/ \
    --libdir=lib/ \
    builddir && \
    ninja -C builddir && \
    ninja -C builddir install && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer/ ninja -C builddir install

WORKDIR /

# Build the gstreamer bad plugins
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-plugins-bad/gst-plugins-bad-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-plugins-bad-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-plugins-bad-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-plugins-bad-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-plugins-bad-${GST_VERSION}

RUN \
    meson setup \
    -Dgpl=enabled \
    -Dva=enabled \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Ddoc=disabled \
    -Dnls=disabled \
    -Dneon=disabled \
    -Ddirectfb=disabled \
    -Dopenni2=disabled \
    -Dfdkaac=disabled \
    -Dladspa=disabled \
    -Dassrender=disabled \
    -Dbs2b=disabled \
    -Dflite=disabled \
    -Drtmp=disabled \
    -Dsbc=disabled \
    -Dteletext=disabled \
    -Dhls-crypto=openssl \
    -Dlibde265=enabled \
    -Dopenh264=enabled \
    -Duvch264=enabled \
    -Dx265=enabled \
    -Dcurl=enabled \
    -Dcurl-ssh2=enabled \
    -Dopus=enabled \
    -Ddtls=enabled \
    -Dsrtp=enabled \
    -Dwebrtc=enabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build the gstreamer ugly plugins
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-plugins-ugly/gst-plugins-ugly-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-plugins-ugly-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-plugins-ugly-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-plugins-ugly-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-plugins-ugly-${GST_VERSION}

RUN \
    meson setup \
    -Dtests=disabled \
    -Dnls=disabled \
    -Dx264=enabled \
    -Dgpl=enabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

#Build ffmpeg
RUN \
    mkdir -p /src/ffmpeg && \
    wget -q --no-check-certificate https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.gz -O /src/ffmpeg/ffmpeg-${FFMPEG_VERSION}.tar.gz && \
    tar -xf /src/ffmpeg/ffmpeg-${FFMPEG_VERSION}.tar.gz -C /src/ffmpeg && \
    rm /src/ffmpeg/ffmpeg-${FFMPEG_VERSION}.tar.gz

WORKDIR /src/ffmpeg/ffmpeg-${FFMPEG_VERSION}

RUN \
    ./configure \
    --enable-pic \
    --enable-shared \
    --enable-static \
    --enable-avfilter \
    --enable-vaapi \
    --extra-cflags="-I/include" \
    --extra-ldflags="-L/lib" \
    --extra-libs=-lpthread \
    --extra-libs=-lm \
    --bindir="/bin" && \
    make -j "$(nproc)" && \
    make install

# Build the gstreamer libav
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-libav/gst-libav-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-libav-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-libav-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-libav-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-libav-${GST_VERSION}

RUN \
    meson setup \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build the gstreamer vaapi
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gstreamer-vaapi/gstreamer-vaapi-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gstreamer-vaapi-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gstreamer-vaapi-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gstreamer-vaapi-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gstreamer-vaapi-${GST_VERSION}

RUN \
    meson setup \
    -Dexamples=disabled \
    -Dtests=disabled \
    -Dencoders=enabled \
    -Ddrm=enabled \
    -Dglx=enabled \
    -Dwayland=enabled \
    -Degl=enabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build the gst python
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-python/gst-python-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-python-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-python-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-python-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-python-${GST_VERSION}

RUN \
    meson setup \
    -Dpython=python3 \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Build the gst rtsp server
RUN \
    wget --quiet --no-check-certificate ${GST_REPO}/gst-rtsp-server/gst-rtsp-server-${GST_VERSION}.tar.xz -O "$GSTREAMER_SRC_DIR"/gst-rtsp-server-${GST_VERSION}.tar.xz && \
    tar -xf "$GSTREAMER_SRC_DIR"/gst-rtsp-server-${GST_VERSION}.tar.xz -C "$GSTREAMER_SRC_DIR" && \
    rm "$GSTREAMER_SRC_DIR"/gst-rtsp-server-${GST_VERSION}.tar.xz

WORKDIR "$GSTREAMER_SRC_DIR"/gst-rtsp-server-${GST_VERSION}

RUN \
    meson setup \
    -Dexamples=disabled \
    -Dtests=disabled \
    --buildtype=debug \
    --prefix=/ \
    --libdir=lib/ \
    --libexecdir=bin/ \
    build/ && \
    ninja -C build && \
    meson install -C build/ && \
    DESTDIR=/opt/intel/dlstreamer/gstreamer meson install -C build/

# Installing gst-rswebrtc-plugins
ENV RUSTFLAGS="-L ${DLSTREAMER_DIR}/gstreamer/lib"

WORKDIR "$GSTREAMER_SRC_DIR"/gst-plugins-rs
# hadolint ignore=SC1091
RUN \
    git clone https://gitlab.freedesktop.org/gstreamer/gst-plugins-rs.git && \
    shopt -s dotglob && \
    mv gst-plugins-rs/* . && \
    git checkout 207196a0334da74c4db9db7c565d882cb9ebc07d && \
    wget -q --no-check-certificate -O- https://sh.rustup.rs | sh -s -- -y --default-toolchain 1.85.0 && \
    source "$HOME"/.cargo/env && \
    cargo install cargo-c --version=0.10.11 && \
    cargo update && \
    cargo cinstall -p gst-plugin-webrtc -p gst-plugin-rtp --libdir="${DLSTREAMER_DIR}"/gstreamer/lib/ && \
    rm "${DLSTREAMER_DIR}"/gstreamer/lib/gstreamer-1.0/libgstrs*.a && \
    rustup self uninstall -y && \
    rm -rf ./* && \
    strip -g "${DLSTREAMER_DIR}"/gstreamer/lib/gstreamer-1.0/libgstrs*.so

# Intel® Distribution of OpenVINO™ Toolkit
RUN \
    wget -q --no-check-certificate https://storage.openvinotoolkit.org/repositories/openvino/packages/"$OPENVINO_VERSION"/linux/"$OPENVINO_FILENAME".tgz && \
    tar -xf "$OPENVINO_FILENAME".tgz && \
    mv "$OPENVINO_FILENAME" /opt/intel/openvino_"$OPENVINO_VERSION".0 && \
    rm "$OPENVINO_FILENAME".tgz && \
    /opt/intel/openvino_"$OPENVINO_VERSION".0/install_dependencies/install_openvino_dependencies.sh -y

# OpenCV
WORKDIR /

RUN \
    wget -q --no-check-certificate -O opencv.zip https://github.com/opencv/opencv/archive/4.10.0.zip && \
    unzip opencv.zip && \
    rm opencv.zip && \
    mv opencv-4.10.0 opencv && \
    mkdir -p opencv/build

WORKDIR /opencv/build

RUN \
    cmake \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_opencv_apps=OFF \
    -GNinja .. && \
    ninja -j "$(nproc)" && \
    ninja install

# Intel® DL Streamer
WORKDIR "$INTEL_DLSTREAMER_DIR"

RUN \
    wget -q --no-check-certificate https://github.com/dlstreamer/dlstreamer/archive/refs/tags/v"$DLSTREAMER_VERSION".zip && \
    unzip v"$DLSTREAMER_VERSION".zip && \
    rm v"$DLSTREAMER_VERSION".zip && \
    mv dlstreamer-"$DLSTREAMER_VERSION"/* . && \
    rm -rf dlstreamer-"$DLSTREAMER_VERSION" && \
    wget -q --no-check-certificate https://github.com/gabime/spdlog/archive/"$SPDLOG_COMMIT".zip && \
    unzip "$SPDLOG_COMMIT".zip && \
    rm "$SPDLOG_COMMIT".zip && \
    mv spdlog-"$SPDLOG_COMMIT"/* thirdparty/spdlog/ && \
    rm -rf spdlog-"$SPDLOG_COMMIT" && \
    wget -q --no-check-certificate https://github.com/google/googletest/archive/"$GOOGLETEST_COMMIT".zip && \
    unzip "$GOOGLETEST_COMMIT".zip && \
    rm "$GOOGLETEST_COMMIT".zip && \
    mkdir thirdparty/googletest && \
    mv googletest-"$GOOGLETEST_COMMIT"/* thirdparty/googletest/ && \
    rm -rf googletest-"$GOOGLETEST_COMMIT" && \
    ${INTEL_DLSTREAMER_DIR}/scripts/install_metapublish_dependencies.sh && \
    mkdir build

WORKDIR "$INTEL_DLSTREAMER_DIR"/build

# Setup enviroment variables using installed packages
# hadolint ignore=SC1091
RUN \
    source /opt/intel/openvino_"$OPENVINO_VERSION".0/setupvars.sh

# setup vars
ENV GSTREAMER_DIR=$DLSTREAMER_DIR/gstreamer
ENV LIBDIR=/opt/intel/dlstreamer/lib
ENV BINDIR=/opt/intel/dlstreamer/build/intel64/Debug/bin
ENV GST_PLUGIN_SCANNER=${BINDIR}/gstreamer-1.0/gst-plugin-scanner
ENV PATH=${BINDIR}:${PATH}
ENV PKG_CONFIG_PATH=${LIBDIR}/pkgconfig:${PKG_CONFIG_PATH}
ENV GI_TYPELIB_PATH=${LIBDIR}/girepository-1.0
ENV LIBRARY_PATH=${LIBDIR}:${LIBRARY_PATH}
ENV LD_LIBRARY_PATH=${LIBDIR}:${LD_LIBRARY_PATH}

ENV LIB_PATH="$LIBDIR"
ENV INTEL_OPENVINO_DIR=/opt/intel/openvino_"$OPENVINO_VERSION".0

# OpenVINO environment variables
ENV OpenVINO_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV InferenceEngine_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV ngraph_DIR="$INTEL_OPENVINO_DIR/runtime/cmake"
ENV HDDL_INSTALL_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl"
ENV TBB_DIR="$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/cmake"
ENV LD_LIBRARY_PATH="$INTEL_OPENVINO_DIR/tools/compile_tool:$INTEL_OPENVINO_DIR/runtime/3rdparty/tbb/lib:$INTEL_OPENVINO_DIR/runtime/3rdparty/hddl/lib:$INTEL_OPENVINO_DIR/runtime/lib/intel64:$LD_LIBRARY_PATH"
ENV PYTHONPATH="$INTEL_OPENVINO_DIR/python/${PYTHON_VERSION}:$PYTHONPATH"

# DL Streamer environment variables
ENV GSTREAMER_DIR="${DLSTREAMER_DIR}/gstreamer"
ENV GST_PLUGIN_PATH=/opt/intel/dlstreamer/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/usr/lib/x86_64-linux-gnu/gstreamer-1.0:/opt/intel/dlstreamer/build/intel64/Debug/lib:
ENV LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/lib/gstreamer-1.0:/usr/lib:${LIBRARY_PATH}"
ENV LD_LIBRARY_PATH="${DLSTREAMER_DIR}/lib:${DLSTREAMER_DIR}/lib/gstreamer-1.0:/usr/lib:${LD_LIBRARY_PATH}"
ENV PKG_CONFIG_PATH="${DLSTREAMER_DIR}/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH}"
ENV MODELS_PATH="${MODELS_PATH:-/home/dlstreamer/intel/dl_streamer/models}"
ENV LC_NUMERIC="C"
ENV C_INCLUDE_PATH="${DLSTREAMER_DIR}/include:${DLSTREAMER_DIR}/include/dlstreamer/gst/metadata:${C_INCLUDE_PATH}"
ENV CPLUS_INCLUDE_PATH="${DLSTREAMER_DIR}/include:${DLSTREAMER_DIR}/include/dlstreamer/gst/metadata:${CPLUS_INCLUDE_PATH}"

# if USE_CUSTOM_GSTREAMER set, add GStreamer build to GST_PLUGIN_SCANNER and PATH
ARG USE_CUSTOM_GSTREAMER=yes
ENV GST_PLUGIN_SCANNER="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/bin/gstreamer-1.0/gst-plugin-scanner}"
ENV GI_TYPELIB_PATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/girepository-1.0}"
ENV PATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/bin:}${PATH}"
ENV PKG_CONFIG_PATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/pkgconfig:}${PKG_CONFIG_PATH}"
ENV LIBRARY_PATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib:}$LIBRARY_PATH"
ENV LD_LIBRARY_PATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib:}$LD_LIBRARY_PATH"
ENV PYTHONPATH="${USE_CUSTOM_GSTREAMER:+${GSTREAMER_DIR}/lib/python3/dist-packages:}$PYTHONPATH"

ENV PYTHONPATH=/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:/home/dlstreamer/dlstreamer/python:${PYTHONPATH}

# Build and install DL Streamer
RUN \
    cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_PAHO_INSTALLATION=ON \
    -DENABLE_RDKAFKA_INSTALLATION=ON \
    -DENABLE_VAAPI=ON \
    -DENABLE_SAMPLES=ON \
    .. && \
    make -j "$(nproc)" && \
    make install && \
    usermod -a -G video dlstreamer && \
    ln -s /opt/intel/dlstreamer /home/dlstreamer/dlstreamer && \
    ln -s /usr/local/lib/gstreamer-1.0 /opt/intel/dlstreamer/lib

WORKDIR /home/dlstreamer
USER dlstreamer

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
  CMD [ "bash", "-c", "pgrep bash > /dev/null || exit 1" ]

CMD ["/bin/bash"]
