# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
ARG dldt=dldt-internal
ARG gst=gst-internal

FROM ubuntu:18.04 AS base
WORKDIR /home

# COMMON BUILD TOOLS
RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends cmake build-essential automake autoconf libtool make git wget pciutils cpio libtool lsb-release ca-certificates pkg-config bison flex libcurl4-gnutls-dev zlib1g-dev

# Build x264
ARG X264_VER=stable
ARG X264_REPO=https://github.com/mirror/x264

RUN apt-get update && apt-get install -y -q --no-install-recommends nasm yasm

RUN  git clone ${X264_REPO} && \
    cd x264 && \
    git checkout ${X264_VER} && \
    ./configure --prefix="/usr" --libdir=/usr/lib/x86_64-linux-gnu --enable-shared && \
    make -j $(nproc) && \
    make install DESTDIR="/home/build" && \
    make install

RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends libx11-dev xorg-dev libgl1-mesa-dev openbox

# Build Intel(R) Media SDK
ARG MSDK_REPO=https://github.com/Intel-Media-SDK/MediaSDK/releases/download/intel-mediasdk-19.1.0/MediaStack.tar.gz

RUN wget -O - ${MSDK_REPO} | tar xz && \
    cd MediaStack && \
    \
    cp -r opt/ /home/build && \
    cp -r etc/ /home/build && \
    \
    cp -a opt/. /opt/ && \
    cp -a etc/. /opt/ && \
    ldconfig

ENV PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/mediasdk/lib64/pkgconfig
ENV LIBRARY_PATH=/opt/intel/mediasdk/lib64:/usr/lib:${LIBRARY_PATH}
ENV LIBVA_DRIVERS_PATH=/opt/intel/mediasdk/lib64
ENV LIBVA_DRIVER_NAME=iHD
ENV GST_VAAPI_ALL_DRIVERS=1

#clinfo needs to be installed after build directory is copied over
RUN mkdir neo && cd neo && \
    wget https://github.com/intel/compute-runtime/releases/download/19.31.13700/intel-gmmlib_19.2.3_amd64.deb && \
    wget https://github.com/intel/compute-runtime/releases/download/19.31.13700/intel-igc-core_1.0.10-2364_amd64.deb && \
    wget https://github.com/intel/compute-runtime/releases/download/19.31.13700/intel-igc-opencl_1.0.10-2364_amd64.deb && \
    wget https://github.com/intel/compute-runtime/releases/download/19.31.13700/intel-opencl_19.31.13700_amd64.deb && \
    wget https://github.com/intel/compute-runtime/releases/download/19.31.13700/intel-ocloc_19.31.13700_amd64.deb && \
    dpkg -i *.deb && \
    dpkg-deb -x intel-gmmlib_19.2.3_amd64.deb /home/build/ && \
    dpkg-deb -x intel-igc-core_1.0.10-2364_amd64.deb /home/build/ && \
    dpkg-deb -x intel-igc-opencl_1.0.10-2364_amd64.deb /home/build/ && \
    dpkg-deb -x intel-opencl_19.31.13700_amd64.deb /home/build/ && \
    dpkg-deb -x intel-ocloc_19.31.13700_amd64.deb /home/build/ && \
    cp -a /home/build/. /

FROM base AS dldt-internal
WORKDIR /home

ARG DLDT_VER=2020.1
ARG DLDT_REPO=https://github.com/opencv/dldt.git

RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends libusb-1.0-0-dev libboost-all-dev libgtk2.0-dev python3 python3-pip python3-setuptools python-yaml

RUN git clone -b ${DLDT_VER} ${DLDT_REPO} && \
    cd dldt && \
    git submodule init && \
    git submodule update --recursive && \
    mkdir build && \
    cd build && \
    cmake -DENABLE_VALIDATION_SET=OFF -DCMAKE_INSTALL_PREFIX=/opt/intel/dldt -DLIB_INSTALL_PATH=/opt/intel/dldt -DENABLE_MKL_DNN=ON -DENABLE_CLDNN=ON -DENABLE_SAMPLE_CORE=OFF  .. && \
    make -j $(nproc) && \
    rm -rf ../bin/intel64/Release/lib/libgtest* && \
    rm -rf ../bin/intel64/Release/lib/libgmock* && \
    rm -rf ../bin/intel64/Release/lib/libmock* && \
    rm -rf ../bin/intel64/Release/lib/libtest*

# Delete TBB debug libs to prevent CI testing failing with TBB asserts
RUN find /home/dldt/inference-engine/temp/tbb/lib/ -name "*tbb*debug*so*" -exec rm -rf {} \;

ARG libdir=/opt/intel/dldt/inference-engine/lib/intel64

RUN mkdir -p    build/opt/intel/dldt/inference-engine/include && \
    cp -r       dldt/inference-engine/include/*                     build/opt/intel/dldt/inference-engine/include && \
    \
    mkdir -p    build/${libdir} && \
    cp -r       dldt/bin/intel64/Release/lib/*     build${libdir} && \
    \
    mkdir -p    build/opt/intel/dldt/inference-engine/src && \
    cp -r       dldt/inference-engine/src/*                         build/opt/intel/dldt/inference-engine/src/ && \
    \
    mkdir -p    build/opt/intel/dldt/inference-engine/share && \
    cp -r       dldt/build/share/*                 build/opt/intel/dldt/inference-engine/share/ && \
    \
    mkdir -p    build/opt/intel/dldt/inference-engine/external/ && \
    mv          dldt/inference-engine/temp/opencv_*_ubuntu18        dldt/inference-engine/temp/opencv && \
    cp -r       dldt/inference-engine/temp/*                        build/opt/intel/dldt/inference-engine/external

RUN for p in /usr /home/build/usr /opt/intel/dldt/inference-engine /home/build/opt/intel/dldt/inference-engine; do \
    pkgconfiglibdir="$p/lib/x86_64-linux-gnu" && \
    mkdir -p "${pkgconfiglibdir}/pkgconfig" && \
    pc="${pkgconfiglibdir}/pkgconfig/dldt.pc" && \
    echo "prefix=/opt" > "$pc" && \
    echo "libdir=${libdir}" >> "$pc" && \
    echo "includedir=/opt/intel/dldt/inference-engine/include" >> "$pc" && \
    echo "" >> "$pc" && \
    echo "Name: DLDT" >> "$pc" && \
    echo "Description: Intel Deep Learning Deployment Toolkit" >> "$pc" && \
    echo "Version: 5.0" >> "$pc" && \
    echo "" >> "$pc" && \
    echo "Libs: -L\${libdir} -linference_engine -linference_engine_c_wrapper" >> "$pc" && \
    echo "Cflags: -I\${includedir}" >> "$pc"; \
    done;

ENV InferenceEngine_DIR=/opt/intel/dldt/inference-engine/share
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/dldt/inference-engine/lib:/opt/intel/dldt/inference-engine/external/tbb/lib:${libdir}
ENV HDDL_INSTALL_DIR=/opt/intel/dldt/inference-engine/external/hddl

FROM base AS gst-internal
WORKDIR /home
# Build the gstreamer core

# TODO: If you step up this version to version 'x.y.z', please review gst-plugins-good installation step and remove rtpjitterbuffer patch applying, if patch is contained in gst-plugins-good-'x.y.z'
ARG GST_VER=1.16.0
ARG GST_REPO=https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-${GST_VER}.tar.xz

RUN  DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends libglib2.0-dev gobject-introspection libgirepository1.0-dev libpango-1.0-0 libpangocairo-1.0-0 autopoint
RUN  wget -O - ${GST_REPO} | tar xJ && \
    cd gstreamer-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --enable-introspection \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install;

# Build the gstreamer plugin bad set
ARG GST_ORC_VER=0.4.29
ARG GST_ORC_REPO=https://gstreamer.freedesktop.org/src/orc/orc-${GST_ORC_VER}.tar.xz

RUN  wget -O - ${GST_ORC_REPO} | tar xJ && \
    cd orc-${GST_ORC_VER} && \
    ./autogen.sh --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

RUN  apt-get update && apt-get install -y -q --no-install-recommends libxrandr-dev libegl1-mesa-dev autopoint bison flex libudev-dev

# Build the gstreamer plugin base
ARG GST_PLUGIN_BASE_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-base/gst-plugins-base-${GST_VER}.tar.xz

RUN  DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends libxv-dev libvisual-0.4-dev libtheora-dev libglib2.0-dev libasound2-dev libcdparanoia-dev libpango1.0-dev

RUN  wget -O - ${GST_PLUGIN_BASE_REPO} | tar xJ && \
    cd gst-plugins-base-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-introspection \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install


# Build the gstreamer plugin good set

# Lines below extract patch needed for Smart City Sample (OVS use case). Patch is applied before building gst-plugins-good
RUN  mkdir gst-plugins-good-${GST_VER} && \
    git clone https://github.com/GStreamer/gst-plugins-good.git && \
    cd gst-plugins-good && \
    git diff 080eba64de68161026f2b451033d6b455cb92a05 37d22186ffb29a830e8aad2e4d6456484e716fe7 > ../gst-plugins-good-${GST_VER}/rtpjitterbuffer-fix.patch

ARG GST_PLUGIN_GOOD_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-${GST_VER}.tar.xz

RUN  apt-get update && apt-get install -y -q --no-install-recommends libsoup2.4-dev libjpeg-dev

RUN  wget -O - ${GST_PLUGIN_GOOD_REPO} | tar xJ && \
    cd gst-plugins-good-${GST_VER} && \
    patch -p1 < rtpjitterbuffer-fix.patch && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install


# Build the gstreamer plugin bad set
ARG GST_PLUGIN_BAD_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-bad/gst-plugins-bad-${GST_VER}.tar.xz

RUN  apt-get update && apt-get install -y -q --no-install-recommends libssl-dev

RUN  wget -O - ${GST_PLUGIN_BAD_REPO} | tar xJ && \
    cd gst-plugins-bad-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build the gstreamer plugin ugly set
ARG GST_PLUGIN_UGLY_REPO=https://gstreamer.freedesktop.org/src/gst-plugins-ugly/gst-plugins-ugly-${GST_VER}.tar.xz

RUN  wget -O - ${GST_PLUGIN_UGLY_REPO} | tar xJ; \
    cd gst-plugins-ugly-${GST_VER}; \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples  \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build gst-libav
ARG GST_PLUGIN_LIBAV_REPO=https://gstreamer.freedesktop.org/src/gst-libav/gst-libav-${GST_VER}.tar.xz

RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends libssl-dev

RUN wget -O - ${GST_PLUGIN_LIBAV_REPO} | tar xJ && \
    cd gst-libav-${GST_VER} && \
    ./autogen.sh \
    --prefix="/usr" \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --enable-gpl \
    --disable-gtk-doc && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# Build gstreamer plugin vaapi
ARG GST_PLUGIN_VAAPI_REPO=https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-${GST_VER}.tar.xz

COPY ./patches/gstreamer-vaapi /patches/gstreamer-vaapi

RUN  wget -O - ${GST_PLUGIN_VAAPI_REPO} | tar xJ && \
    cd gstreamer-vaapi-${GST_VER} && git apply /patches/gstreamer-vaapi/vasurface_qdata.patch && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --enable-shared \
    --disable-examples \
    --disable-gtk-doc  && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

# gst-python
RUN apt-get update \
    && apt-get install --no-install-recommends -y \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python-gi-dev \
    curl \
    inetutils-ping \
    libsoup2.4.1

ARG GST_PYTHON_REPO=https://gstreamer.freedesktop.org/src/gst-python/gst-python-${GST_VER}.tar.xz
RUN  wget -O - ${GST_PYTHON_REPO} | tar xJ && \
    cd gst-python-${GST_VER} && \
    ./autogen.sh \
    --prefix=/usr \
    --libdir=/usr/lib/x86_64-linux-gnu \
    --libexecdir=/usr/lib/x86_64-linux-gnu \
    --with-pygi-overrides-dir=/usr/lib/python3/dist-packages/gi/overrides \
    --disable-dependency-tracking \
    --disable-silent-rules \
    --with-libpython-dir="/usr/lib/x86_64-linux-gnu/" \
    PYTHON=/usr/bin/python3 && \
    make -j $(nproc) && \
    make install DESTDIR=/home/build && \
    make install

RUN apt-get install -y -q --no-install-recommends gtk-doc-tools

ARG ENABLE_PAHO_INSTALLATION=false
ARG PAHO_VER=1.3.0
ARG PAHO_REPO=https://github.com/eclipse/paho.mqtt.c/archive/v${PAHO_VER}.tar.gz
RUN if [ "$ENABLE_PAHO_INSTALLATION" = "true" ] ; then \
    wget -O - ${PAHO_REPO} | tar -xz && \
    cd paho.mqtt.c-${PAHO_VER} && \
    make && \
    make install && \
    cp build/output/libpaho-mqtt3c.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/ && \
    cp build/output/libpaho-mqtt3cs.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/ && \
    cp build/output/libpaho-mqtt3a.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/ && \
    cp build/output/libpaho-mqtt3as.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/ && \
    cp build/output/paho_c_version /home/build/usr/bin/ && \
    cp build/output/samples/paho_c_pub /home/build/usr/bin/ && \
    cp build/output/samples/paho_c_sub /home/build/usr/bin/ && \
    cp build/output/samples/paho_cs_pub /home/build/usr/bin/ && \
    cp build/output/samples/paho_cs_sub /home/build/usr/bin/ && \
    chmod 644 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3c.so.1.0 && \
    chmod 644 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3cs.so.1.0 && \
    chmod 644 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3a.so.1.0 && \
    chmod 644 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so.1.0 && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3c.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3c.so.1 && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3cs.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3cs.so.1 && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3a.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3a.so.1 && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so.1.0 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so.1 && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3c.so.1 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3c.so && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3cs.so.1 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3cs.so && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3a.so.1 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3a.so && \
    ln /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so.1 /home/build/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so && \
    cp src/MQTTAsync.h /home/build/usr/include/ && \
    cp src/MQTTClient.h /home/build/usr/include/ && \
    cp src/MQTTClientPersistence.h /home/build/usr/include/ && \
    cp src/MQTTProperties.h /home/build/usr/include/ && \
    cp src/MQTTReasonCodes.h /home/build/usr/include/ && \
    cp src/MQTTSubscribeOpts.h /home/build/usr/include/; \
    else \
    echo "PAHO install disabled"; \
    fi

ARG ENABLE_RDKAFKA_INSTALLATION=false
ARG RDKAFKA_VER=1.0.0
ARG RDKAFKA_REPO=https://github.com/edenhill/librdkafka/archive/v${RDKAFKA_VER}.tar.gz
RUN if [ "$ENABLE_RDKAFKA_INSTALLATION" = "true" ] ; then \
    wget -O - ${RDKAFKA_REPO} | tar -xz && \
    cd librdkafka-${RDKAFKA_VER} && \
    ./configure --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu/ && \
    make && \
    make install && \
    make install DESTDIR=/home/build; \
    else \
    echo "KAFKA install disabled"; \
    fi



FROM ${dldt} AS dldt-build

FROM ${gst} AS gst-build


FROM ubuntu:18.04
LABEL Description="This is the base image for GSTREAMER & DLDT Ubuntu 18.04 LTS"
LABEL Vendor="Intel Corporation"
WORKDIR /root

# Prerequisites
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends libxv1 libxcb-shm0 libxcb-shape0 libxcb-xfixes0 libsdl2-2.0-0 libasound2 libvdpau1 \
    libnuma1 libass9 libssl1.0.0 libglib2.0 libpango-1.0-0 libpangocairo-1.0-0 gobject-introspection libudev1 libx11-xcb1 libgl1-mesa-glx libxrandr2 libegl1-mesa \
    libpng16-16 libvisual-0.4-0 libtheora0 libcdparanoia0 libsoup2.4-1 libjpeg8 libjpeg-turbo8 python3 python3-pip python-yaml \
    libgtk2.0 clinfo \
    \
    libusb-1.0-0-dev libboost-all-dev libjson-c-dev \
    build-essential cmake ocl-icd-opencl-dev wget gcovr vim git gdb ca-certificates libssl-dev uuid-dev \
    libgirepository-1.0-1 \
    python3 python3-wheel python3-pip python3-setuptools python-gi-dev python3.6-dev \
    && rm -rf /var/lib/apt/lists/* \
    && python3.6 -m pip install numpy opencv-python

# Install
COPY --from=dldt-build /home/build /
COPY --from=gst-build /home/build /

RUN echo "\
    /usr/local/lib\n\
    /usr/lib/x86_64-linux-gnu/gstreamer-1.0\n\
    /opt/intel/dldt/inference-engine/lib/intel64/\n\
    /opt/intel/dldt/inference-engine/external/tbb/lib\n\
    /opt/intel/dldt/inference-engine/external/mkltiny_lnx/lib\n\
    /opt/intel/dldt/inference-engine/external/vpu/hddl/lib\n\
    /opt/intel/dldt/inference-engine/external/opencv/opencv/lib/" > /etc/ld.so.conf.d/opencv-dldt-gst.conf && ldconfig

ENV PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/mediasdk/lib64/pkgconfig:${PKG_CONFIG_PATH}
ENV InferenceEngine_DIR=/opt/intel/dldt/inference-engine/share
ENV OpenCV_DIR=/opt/intel/dldt/inference-engine/external/opencv/opencv/cmake
ENV LIBRARY_PATH=/opt/intel/mediasdk/lib64:/usr/lib:${LIBRARY_PATH}
ENV PATH=/usr/bin:/opt/intel/mediasdk/bin:${PATH}

ENV LIBVA_DRIVERS_PATH=/opt/intel/mediasdk/lib64
ENV LIBVA_DRIVER_NAME=iHD
ENV GST_VAAPI_ALL_DRIVERS=1
ENV DISPLAY=:0.0
ENV LD_LIBRARY_PATH=/opt/intel/dldt/inference-engine/external/hddl/lib
ENV HDDL_INSTALL_DIR=/opt/intel/dldt/inference-engine/external/hddl

ARG GIT_INFO
ARG SOURCE_REV

COPY . gst-video-analytics
ARG ENABLE_PAHO_INSTALLATION=false
ARG ENABLE_RDKAFKA_INSTALLATION=false
ARG EXTERNAL_GVA_BUILD_FLAGS

RUN mkdir -p gst-video-analytics/build \
    && cd gst-video-analytics/build \
    && cmake \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVERSION_PATCH=${SOURCE_REV} \
    -DGIT_INFO=${GIT_INFO} \
    -DBUILD_SHARED_LIBS=ON \
    -DENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION} \
    -DENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION} \
    -DHAVE_VAAPI=ON \
    -DENABLE_VAS_TRACKER=ON \
    ${EXTERNAL_GVA_BUILD_FLAGS} \
    .. \
    && make -j $(nproc) \
    && make install \
    && echo "/usr/lib/gst-video-analytics" >> /etc/ld.so.conf.d/opencv-dldt-gst.conf && ldconfig
ENV GST_PLUGIN_PATH=/usr/lib/gst-video-analytics/:/root/gst-video-analytics/
ENV PYTHONPATH=/root/gst-video-analytics/python:$PYTHONPATH
