# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

FROM ubuntu:22.04

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

RUN \
    apt-get update && \
    apt-get install -y -q --no-install-recommends gnupg=\* ca-certificates=\* wget=\* libtbb-dev=\* cmake=\* git=\* git-lfs=\* vim=\* numactl=\* && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Intel® NPU drivers (optional)
RUN \
    mkdir debs && \
    dpkg --purge --force-remove-reinstreq intel-driver-compiler-npu intel-fw-npu intel-level-zero-npu level-zero && \
    wget -q https://github.com/oneapi-src/level-zero/releases/download/v1.17.44/level-zero_1.17.44+u22.04_amd64.deb -P ./debs && \
    wget -q --no-check-certificate -nH --accept-regex="ubuntu22" --cut-dirs=5 -r https://github.com/intel/linux-npu-driver/releases/expanded_assets/v1.13.0 -P ./debs && \
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

RUN \
    echo "deb https://apt.repos.intel.com/openvino/2025 ubuntu22 main" | tee /etc/apt/sources.list.d/intel-openvino-2025.list && \
    wget -q https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
    apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB && \
    wget -q -O- https://eci.intel.com/sed-repos/gpg-keys/GPG-PUB-KEY-INTEL-SED.gpg | tee /usr/share/keyrings/sed-archive-keyring.gpg > /dev/null && \
    echo "deb [signed-by=/usr/share/keyrings/sed-archive-keyring.gpg] https://eci.intel.com/sed-repos/jammy sed main" | tee /etc/apt/sources.list.d/sed.list && \
    echo "deb-src [signed-by=/usr/share/keyrings/sed-archive-keyring.gpg] https://eci.intel.com/sed-repos/jammy sed main" | tee -a /etc/apt/sources.list.d/sed.list && \
    bash -c 'echo -e "Package: *\nPin: origin eci.intel.com\nPin-Priority: 1000" > /etc/apt/preferences.d/sed'

ARG DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update -y && \
    apt-get install -y -q --no-install-recommends intel-dlstreamer=\* && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* && \
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
    /python3venv/bin/pip3 install --no-cache-dir --no-dependencies PyGObject==3.50.0 setuptools==75.8.0
  
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD [ "bash", "-c", "pgrep bash > /dev/null || exit 1" ]

CMD ["/bin/bash"]
