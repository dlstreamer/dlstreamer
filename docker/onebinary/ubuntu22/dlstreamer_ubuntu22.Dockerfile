# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

FROM ubuntu:22.04

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

RUN \
    apt-get update && \
    apt-get install -y -q --no-install-recommends gnupg=\* ca-certificates=\* wget=\* libtbb-dev=\* cmake=\* vim=\* numactl=\* && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN wget -q https://raw.githubusercontent.com/open-edge-platform/edge-ai-libraries/main/libraries/dl-streamer/scripts/DLS_install_prerequisites.sh && \
    chmod +x DLS_install_prerequisites.sh && \
    ./DLS_install_prerequisites.sh --on-host-or-docker=docker_ubuntu22 && \
    rm -f DLS_install_prerequisites.sh

RUN \
    echo "deb https://apt.repos.intel.com/openvino/2025 ubuntu22 main" | tee /etc/apt/sources.list.d/intel-openvino-2025.list && \
    wget -q https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
    apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB

RUN mkdir -p /debs
COPY ./deb_packages/*.deb /debs/

ARG DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update -y && \
    apt-get install -y -q --no-install-recommends /debs/*.deb && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* && \
    rm -rf /debs && \
    useradd -ms /bin/bash dlstreamer && \
    chown -R dlstreamer: /opt && \
    chmod -R u+rw /opt

RUN \
    mkdir /python3venv && \
    chown -R dlstreamer: /python3venv && \
    chmod -R u+w /python3venv

ENV LIBVA_DRIVER_NAME=iHD
ENV GST_PLUGIN_PATH=/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/opt/intel/dlstreamer/gstreamer/lib/:
ENV LD_LIBRARY_PATH=/opt/intel/dlstreamer/gstreamer/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/lib/gstreamer-1.0:/usr/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/opencv:/opt/openh264:/opt/rdkafka:/opt/ffmpeg:/usr/local/lib/gstreamer-1.0:/usr/local/lib
ENV LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
ENV GST_VA_ALL_DRIVERS=1
ENV MODEL_PROC_PATH=/opt/intel/dlstreamer/samples/gstreamer/model_proc
ENV PATH=/python3venv/bin:/opt/intel/dlstreamer/gstreamer/bin:/opt/intel/dlstreamer/build/intel64/Release/bin:$PATH
ENV PYTHONPATH=/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:/home/dlstreamer/dlstreamer/python:/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:
ENV TERM=xterm
ENV GI_TYPELIB_PATH=/opt/intel/dlstreamer/gstreamer/lib/girepository-1.0:/usr/lib/x86_64-linux-gnu/girepository-1.0

RUN \
    usermod -a -G video dlstreamer && \
    ln -s /opt/intel/dlstreamer /home/dlstreamer/dlstreamer

WORKDIR /home/dlstreamer
USER dlstreamer

RUN \
    python3 -m venv /python3venv && \
    /python3venv/bin/pip3 install --no-cache-dir --upgrade pip && \
    /python3venv/bin/pip3 install --no-cache-dir --no-dependencies PyGObject==3.50.0 setuptools==78.1.1 numpy==2.2.0 tqdm==4.67.1 opencv-python==4.11.0.86

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD [ "bash", "-c", "pgrep bash > /dev/null || exit 1" ]

CMD ["/bin/bash"]
