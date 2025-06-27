# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

FROM fedora:41

SHELL ["/bin/bash", "-xo", "pipefail", "-c"]

RUN \
    dnf install -y \
    "https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm" \
    "https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm" && \
    dnf clean all

RUN \
    printf "[OpenVINO]\n\
name=Intel(R) Distribution of OpenVINO\n\
baseurl=https://yum.repos.intel.com/openvino\n\
enabled=1\n\
gpgcheck=1\n\
repo_gpgcheck=1\n\
gpgkey=https://yum.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB\n" >/tmp/openvino.repo && \
    mv /tmp/openvino.repo /etc/yum.repos.d

RUN mkdir -p /rpms

COPY ./rpm_packages/*.rpm /rpms/

# Download and install DLS rpm package
RUN \
    dnf install -y /rpms/*.rpm && \
    dnf clean all && \
    useradd -ms /bin/bash dlstreamer && \
    chown -R dlstreamer: /opt && \
    chmod -R u+rw /opt

RUN \
    mkdir /python3venv && \
    chown -R dlstreamer: /python3venv && \
    chmod -R u+w /python3venv

ENV LIBVA_DRIVER_NAME=iHD
ENV GST_PLUGIN_PATH=/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/opt/intel/dlstreamer/gstreamer/lib/
ENV LD_LIBRARY_PATH=/opt/intel/dlstreamer/gstreamer/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/lib/gstreamer-1.0:/usr/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/opencv:/opt/openh264:/opt/rdkafka:/opt/ffmpeg:/usr/local/lib/gstreamer-1.0:/usr/local/lib
ENV LIBVA_DRIVERS_PATH=/usr/lib64/dri-nonfree
ENV GST_VA_ALL_DRIVERS=1
ENV MODEL_PROC_PATH=/opt/intel/dlstreamer/samples/gstreamer/model_proc
ENV PATH=/python3venv/bin:/opt/intel/dlstreamer/gstreamer/bin:/opt/intel/dlstreamer/build/intel64/Release/bin:$PATH
ENV PYTHONPATH=/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:/home/dlstreamer/dlstreamer/python:/opt/intel/dlstreamer/gstreamer/lib/python3/dist-packages:
ENV TERM=xterm
ENV GI_TYPELIB_PATH=/opt/intel/dlstreamer/gstreamer/lib/girepository-1.0:/usr/lib64/girepository-1.0

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
