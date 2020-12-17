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
    && ./install_openvino_dependencies.sh -y \
    && OpenVINO_YEAR="$(echo ${OpenVINO_VERSION} | cut -d "." -f 1)" \
    && sed -i 's/decline/accept/g' silent.cfg \
    && ./install.sh -s silent.cfg \
    && ln --symbolic /opt/intel/openvino_${OpenVINO_VERSION}/ /opt/intel/openvino \
    && cp ./rpm/intel-openvino-mediasdk* /opt/intel/mediasdk/


FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04 as gst-build
ENV HOME=/home
WORKDIR ${HOME}

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

ARG GST_VERSION=1.16.2
ARG BUILD_TYPE=release

ENV GSTREAMER_LIB_DIR=${PREFIX}/${LIBDIR}
ENV LIBRARY_PATH=${GSTREAMER_LIB_DIR}:${GSTREAMER_LIB_DIR}/gstreamer-1.0:${LIBRARY_PATH}
ENV LD_LIBRARY_PATH=${LIBRARY_PATH}
ENV PKG_CONFIG_PATH=${GSTREAMER_LIB_DIR}/pkgconfig
ENV PATCHES_ROOT=${HOME}/build/src/patches
ENV SYS_PATCHES_DIR=${HOME}/src/patches
RUN mkdir -p ${PATCHES_ROOT} && mkdir -p ${SYS_PATCHES_DIR}

# GStreamer core
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install --no-install-recommends -q -y \
        libglib2.0-dev \
        libgmp-dev \
        libgsl-dev \
        gobject-introspection \
        libcap-dev \
        libcap2-bin \
        gettext \
        libssl-dev \
        ffmpeg \
        gstreamer1.0-plugins-base \
        gstreamer1.0-plugins-good \
        gstreamer1.0-plugins-bad \
        gstreamer1.0-libav \
        gstreamer1.0-plugins-ugly \
        gstreamer1.0-alsa \
        gstreamer1.0-pulseaudio \
        libgstreamer1.0-0 \
        libgstreamer1.0-dev \
        libgstreamer-plugins-base1.0-dev \
        libgstreamer-plugins-bad1.0-dev \
        libgirepository1.0-dev && \
    pip3 install --no-cache-dir meson ninja

# ORC Acceleration
ARG MESON_GST_TESTS=disabled

ARG GST_ORC_VERSION=0.4.31
ARG GST_ORC_REPO=https://gstreamer.freedesktop.org/src/orc/orc-${GST_ORC_VERSION}.tar.xz
RUN wget ${GST_ORC_REPO} -O src/orc-${GST_ORC_VERSION}.tar.xz
RUN tar xvf src/orc-${GST_ORC_VERSION}.tar.xz && \
    cd orc-${GST_ORC_VERSION} && \
    meson \
    -Dexamples=disabled \
    -Dtests=${MESON_GST_TESTS} \
    -Dbenchmarks=disabled \
    -Dgtk_doc=disabled \
    -Dorc-test=${MESON_GST_TESTS} \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

ENV LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:${LIBRARY_PATH}
ENV LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
ENV LIBVA_DRIVER_NAME=iHD

# Build gstreamer plugin vaapi
ARG GST_PLUGIN_VAAPI_REPO=https://gstreamer.freedesktop.org/src/gstreamer-vaapi/gstreamer-vaapi-${GST_VERSION}.tar.xz

ENV GST_VAAPI_ALL_DRIVERS=1

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
ARG GSTREAMER_VAAPI_PATCH_URL=https://raw.githubusercontent.com/openvinotoolkit/dlstreamer_gst/master/patches/gstreamer-vaapi/vasurface_qdata.patch
RUN wget ${GSTREAMER_VAAPI_PATCH_URL} -O ${PATCHES_ROOT}/gstreamer-vaapi.patch

# put gstreamer-vaapi license along with the patch
RUN mkdir ${PATCHES_ROOT}/gstreamer_vaapi_patch_license && \
    cp gstreamer-vaapi-${GST_VERSION}/COPYING.LIB ${PATCHES_ROOT}/gstreamer_vaapi_patch_license/LICENSE

RUN cd gstreamer-vaapi-${GST_VERSION} && \
    wget -O - ${GSTREAMER_VAAPI_PATCH_URL} | git apply && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dexamples=${MESON_GST_TESTS} \
    -Dtests=${MESON_GST_TESTS} \
    -Dgtk_doc=disabled \
    -Dnls=disabled \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    DESTDIR=/home/build meson install -C build/ && \
    meson install -C build/

# gst-python
RUN DEBIAN_FRONTEND=noninteractive apt-get update && \
    apt-get install --no-install-recommends -y \
    python-gi-dev \
    python-gobject \
    python3-dev && \
    rm -rf /var/lib/apt/lists/*

# Install gst-rtsp-server
ARG GST_RTSP_SERVER_REPO=https://gstreamer.freedesktop.org/src/gst-rtsp-server/gst-rtsp-server-${GST_VERSION}.tar.xz
RUN wget ${GST_RTSP_SERVER_REPO} -O src/gst-rtsp-server-${GST_VERSION}.tar.xz
RUN tar xf src/gst-rtsp-server-${GST_VERSION}.tar.xz && \
    cd gst-rtsp-server-${GST_VERSION} && \
    PKG_CONFIG_PATH=$PWD/build/pkgconfig:${PKG_CONFIG_PATH} meson \
    -Dexamples=${MESON_GST_TESTS} \
    -Dtests=${MESON_GST_TESTS} \
    -Dpackage-origin="${PACKAGE_ORIGIN}" \
    --buildtype=${BUILD_TYPE} \
    --prefix=${PREFIX} \
    --libdir=${LIBDIR} \
    --libexecdir=${LIBEXECDIR} \
    build/ && \
    ninja -C build && \
    meson install -C build/

# Install paho
ARG ENABLE_PAHO_INSTALLATION=true
ARG PAHO_VER=1.3.4
ARG PAHO_REPO=https://github.com/eclipse/paho.mqtt.c/archive/v${PAHO_VER}.tar.gz
RUN if [ "$ENABLE_PAHO_INSTALLATION" = "true" ] ; then \
    wget -O - ${PAHO_REPO} | tar -xz && \
    cd paho.mqtt.c-${PAHO_VER} && \
    make && \
    make install && \
    cp build/output/libpaho-mqtt3c.so.1.3 /home/build/${LIBDIR}/ && \
    cp build/output/libpaho-mqtt3cs.so.1.3 /home/build/${LIBDIR}/ && \
    cp build/output/libpaho-mqtt3a.so.1.3 /home/build/${LIBDIR}/ && \
    cp build/output/libpaho-mqtt3as.so.1.3 /home/build/${LIBDIR}/ && \
    mkdir -p /home/build/${LIBEXECDIR} && \
    cp build/output/paho_c_version /home/build/${LIBEXECDIR}/ && \
    cp build/output/samples/paho_c_pub /home/build/${LIBEXECDIR}/ && \
    cp build/output/samples/paho_c_sub /home/build/${LIBEXECDIR}/ && \
    cp build/output/samples/paho_cs_pub /home/build/${LIBEXECDIR}/ && \
    cp build/output/samples/paho_cs_sub /home/build/${LIBEXECDIR}/ && \
    chmod 644 /home/build/${LIBDIR}/libpaho-mqtt3c.so.1.3 && \
    chmod 644 /home/build/${LIBDIR}/libpaho-mqtt3cs.so.1.3 && \
    chmod 644 /home/build/${LIBDIR}/libpaho-mqtt3a.so.1.3 && \
    chmod 644 /home/build/${LIBDIR}/libpaho-mqtt3as.so.1.3 && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3c.so.1.3 /home/build/${LIBDIR}/libpaho-mqtt3c.so.1 && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3cs.so.1.3 /home/build/${LIBDIR}/libpaho-mqtt3cs.so.1 && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3a.so.1.3 /home/build/${LIBDIR}/libpaho-mqtt3a.so.1 && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3as.so.1.3 /home/build/${LIBDIR}/libpaho-mqtt3as.so.1 && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3c.so.1 /home/build/${LIBDIR}/libpaho-mqtt3c.so && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3cs.so.1 /home/build/${LIBDIR}/libpaho-mqtt3cs.so && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3a.so.1 /home/build/${LIBDIR}/libpaho-mqtt3a.so && \
    ln /home/build/${LIBDIR}/libpaho-mqtt3as.so.1 /home/build/${LIBDIR}/libpaho-mqtt3as.so && \
    mkdir -p /home/build/${PREFIX}/include/ && \
    cp src/MQTTAsync.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTExportDeclarations.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTClient.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTClientPersistence.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTProperties.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTReasonCodes.h /home/build/${PREFIX}/include/ && \
    cp src/MQTTSubscribeOpts.h /home/build/${PREFIX}/include/; \
    else \
    echo "PAHO install disabled"; \
    fi

# Install rdkafka
ARG ENABLE_RDKAFKA_INSTALLATION=true
ARG RDKAFKA_VER=1.5.0
ARG RDKAFKA_REPO=https://github.com/edenhill/librdkafka/archive/v${RDKAFKA_VER}.tar.gz
RUN if [ "$ENABLE_RDKAFKA_INSTALLATION" = "true" ] ; then \
        wget -O - ${RDKAFKA_REPO} | tar -xz && \
        cd librdkafka-${RDKAFKA_VER} && \
        ./configure \
            --prefix=${PREFIX} \
            --libdir=${GSTREAMER_LIB_DIR} && \
        make -j $(nproc) && \
        make install && \
        make install DESTDIR=/home/build && \
        rm /home/build/lib/librdkafka*.a && \
        rm /home/build/lib/pkgconfig/rdkafka*static.pc; \
    else \
        echo "KAFKA install disabled"; \
    fi

RUN grep -lr "prefix=/" --include="*.pc" -l /home/build/ | xargs sed -i 's#prefix=/#prefix=\${pcfiledir}\/..\/..\/#g' \
    && grep -lr "includedir=/" --include="*.pc" -l /home/build/ | xargs sed -i 's#includedir=/#includedir=\${prefix}\/#g' \
    && grep -lr "libdir=/" --include="*.pc" -l /home/build/ | xargs sed -i 's#libdir=/#libdir=\${prefix}\/#g'

FROM ov-build
FROM gst-build

FROM ${DOCKER_PRIVATE_REGISTRY}ubuntu:20.04
LABEL Description="This is the base image for GSTREAMER & OpenVINO™ Toolkit Ubuntu 20.04 LTS"
LABEL Vendor="Intel Corporation"
WORKDIR /root

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

ENV HTTP_PROXY=${http_proxy}
ENV HTTPS_PROXY=${https_proxy}

# Prerequisites
RUN apt-get update && apt-get upgrade -y && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
    lsb-release python3-yaml python3-wheel python3-pip python3-setuptools python3-dev python-gi-dev git wget curl pkg-config cmake clinfo vainfo gobject-introspection libusb-1.0.0 gnupg2 software-properties-common \
    opencl-headers ocl-icd-opencl-dev gpg-agent gcovr vim gdb ca-certificates uuid-dev libva-dev libva-drm2 ocl-icd-libopencl1\
    \
    libgstreamer1.0-0 gstreamer1.0-dev gstreamer1.0-tools gstreamer1.0-doc gstreamer1.0-plugins-base libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-good  \
    libgstreamer-plugins-good1.0-dev gstreamer1.0-plugins-bad libgstreamer-plugins-bad1.0-dev gstreamer1.0-plugins-ugly gstreamer1.0-libav gstreamer1.0-video \
    libgstrtspserver-1.0-dev python3-gst-1.0 \
    gstreamer1.0-x gstreamer1.0-alsa gstreamer1.0-gl gstreamer1.0-gtk3 gstreamer1.0-qt5 gstreamer1.0-pulseaudio

# Copy
COPY --from=OV-build /opt/intel /opt/intel
COPY --from=gst-build /home/build /gst

# Install NEO OCL drivers
RUN mkdir neo && cd neo \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-gmmlib_20.2.4_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-igc-core_1.0.4756_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-igc-opencl_1.0.4756_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-opencl_20.35.17767_amd64.deb \
    && wget https://github.com/intel/compute-runtime/releases/download/20.35.17767/intel-ocloc_20.35.17767_amd64.deb \
    && dpkg -i intel*.deb

ARG PREFIX=/gst
ARG LIBDIR=lib
ARG LIBEXECDIR=bin/
ARG INCLUDEDIR=include/

ENV GSTREAMER_LIB_DIR=${PREFIX}/${LIBDIR}
ENV GST_PLUGIN_SCANNER=/usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner
ENV C_INCLUDE_PATH=${PREFIX}/${INCLUDEDIR}:${C_INCLUDE_PATH}
ENV CPLUS_INCLUDE_PATH=${PREFIX}/${INCLUDEDIR}:${CPLUS_INCLUDE_PATH}

RUN cp /gst/bin/* /bin \
    && cp -r /gst/include/* /usr/include \
    && cp -r ${GSTREAMER_LIB_DIR}/pkgconfig/* /lib/pkgconfig/ \
    && cp -r ${GSTREAMER_LIB_DIR}/gstreamer-1.0/* /usr/lib/x86_64-linux-gnu/gstreamer-1.0 \
    \
    && cp ${GSTREAMER_LIB_DIR}/libpaho-mqtt3c.so.1.3 /lib/ \
    && cp ${GSTREAMER_LIB_DIR}/libpaho-mqtt3cs.so.1.3 /lib/ \
    && cp ${GSTREAMER_LIB_DIR}/libpaho-mqtt3a.so.1.3 /lib/ \
    && cp ${GSTREAMER_LIB_DIR}/libpaho-mqtt3as.so.1.3 /lib/ \
    && chmod 644 /lib/libpaho-mqtt3c.so.1.3 \
    && chmod 644 /lib/libpaho-mqtt3cs.so.1.3 \
    && chmod 644 /lib/libpaho-mqtt3a.so.1.3 \
    && chmod 644 /lib/libpaho-mqtt3as.so.1.3 \
    && ln /lib/libpaho-mqtt3c.so.1.3 /lib/libpaho-mqtt3c.so.1 \
    && ln /lib/libpaho-mqtt3cs.so.1.3 /lib/libpaho-mqtt3cs.so.1 \
    && ln /lib/libpaho-mqtt3a.so.1.3 /lib/libpaho-mqtt3a.so.1 \
    && ln /lib/libpaho-mqtt3as.so.1.3 /lib/libpaho-mqtt3as.so.1 \
    && ln /lib/libpaho-mqtt3c.so.1 /lib/libpaho-mqtt3c.so \
    && ln /lib/libpaho-mqtt3cs.so.1 /lib/libpaho-mqtt3cs.so \
    && ln /lib/libpaho-mqtt3a.so.1 /lib/libpaho-mqtt3a.so \
    && ln /lib/libpaho-mqtt3as.so.1 /lib/libpaho-mqtt3as.so \
    \
    && cp ${GSTREAMER_LIB_DIR}/librdkafka* /lib/ \
    && mkdir -pv /usr/xdgr

RUN echo "\
    /usr/local/lib\n\
    /usr/lib/x86_64-linux-gnu\n\
    /opt/intel/openvino/inference_engine/lib/intel64\n\
    /opt/intel/openvino/inference_engine/external/tbb/lib\n\
    /opt/intel/openvino/deployment_tools/ngraph/lib\n\
    /opt/intel/openvino/inference_engine/external/hddl/lib\n\
    /opt/intel/openvino/opencv/lib/" > /etc/ld.so.conf.d/opencv-dldt-gst.conf && ldconfig

ENV GI_TYPELIB_PATH=/usr/lib/x86_64-linux-gnu/girepository-1.0

ENV InferenceEngine_DIR=/opt/intel/openvino/inference_engine/share
ENV OpenCV_DIR=/opt/intel/openvino/opencv/cmake
ENV LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib:${LIBRARY_PATH}
ENV PATH=/usr/bin:${PREFIX}/${LIBEXECDIR}:${PATH}
ENV XDG_RUNTIME_DIR=${PATH}:/usr/xdgr

ENV LIBVA_DRIVERS_PATH=/opt/intel/mediasdk/lib64
ENV LD_LIBARY_PATH=/opt/intel/mediasdk/lib64:${LD_LIBRARY_PATH}
ENV LIBVA_DRIVER_NAME=iHD
ENV GST_VAAPI_ALL_DRIVERS=1
ENV DISPLAY=:0.0
ENV HDDL_INSTALL_DIR=/opt/intel/openvino/inference_engine/external/hddl
ENV ngraph_DIR=/opt/intel/openvino/deployment_tools/ngraph/cmake/

ARG GIT_INFO
ARG SOURCE_REV

# Install stable MediaSDK version
RUN add-apt-repository universe \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends alien \
    && alien -i /opt/intel/mediasdk/intel-openvino-mediasdk*.rpm

# Source setupvars
RUN source /opt/intel/openvino/bin/setupvars.sh \
    && printf "\nsource /opt/intel/openvino/bin/setupvars.sh\n" >> /root/.bashrc

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

RUN mkdir -p dl-streamer/build \
    && cd dl-streamer/build \
    && cmake \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DVERSION_PATCH=${SOURCE_REV} \
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

WORKDIR ${OV_DLSTREAMER_DIR}/samples

CMD ["/bin/bash"]
