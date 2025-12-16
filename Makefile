# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

.DEFAULT_GOAL := help
SHELL := /bin/bash
nproc ?= 1

GSTREAMER_TAG 	:= 1.26.4
OPENCV_TAG 		:= 4.6.0

DLSTREAMER_VERSION 	:= 0.0.0
BUILD_ARG 			:= Release

OPENVINO_DIR 		?= /opt/intel/openvino_2025/
PROJECT_DIRECTORY 	:= $(pwd)

PATH 					:= /opt/intel/dlstreamer/gstreamer/bin:${PROJECT_DIRECTORY}/build/intel64/Release/bin:${HOME}/.local/bin:${HOME}/python3venv/bin:${PATH}
GST_PLUGIN_PATH 		:= ${PROJECT_DIRECTORY}/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/usr/lib/x86_64-linux-gnu/gstreamer-1.0
LD_LIBRARY_PATH 		:= /opt/intel/dlstreamer/gstreamer/lib:${PROJECT_DIRECTORY}/build/intel64/Release/lib:/usr/lib:${LD_LIBRARY_PATH}
PKG_CONFIG_PATH 		:= /usr/local/lib/pkgconfig:${PROJECT_DIRECTORY}/build/intel64/Release/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:${PKG_CONFIG_PATH}
LIBVA_DRIVER_NAME 		:= iHD
LIBVA_DRIVERS_PATH 		:= /usr/lib/x86_64-linux-gnu/dri
GST_VA_ALL_DRIVERS 		:= 1
GST_PLUGIN_FEATURE_RANK := ${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

DOCKER_PRIVATE_REGISTRY := # Empty on purpose

.PHONY: gstramer
gstreamer: 
	mkdir -p build
	-cd build && git clone -b ${GSTREAMER_TAG} https://gitlab.freedesktop.org/gstreamer/gstreamer.git
	cd build/gstreamer && meson setup \
		-Dexamples=disabled \
    	-Dtests=disabled \
    	-Dvaapi=enabled \
    	-Dlibnice=enabled \
    	-Dgst-examples=disabled \
    	-Ddevtools=disabled \
    	-Dorc=disabled \
    	-Dgpl=enabled \
    	-Dgst-plugins-base:nls=disabled \
    	-Dgst-plugins-base:gl=disabled \
    	-Dgst-plugins-base:xvideo=enabled \
    	-Dgst-plugins-base:vorbis=enabled \
    	-Dgst-plugins-base:pango=disabled \
    	-Dgst-plugins-good:nls=disabled \
    	-Dgst-plugins-good:libcaca=disabled \
    	-Dgst-plugins-good:vpx=enabled \
    	-Dgst-plugins-good:rtp=enabled \
    	-Dgst-plugins-good:rtpmanager=enabled \
    	-Dgst-plugins-good:adaptivedemux2=disabled \
    	-Dgst-plugins-good:lame=disabled \
    	-Dgst-plugins-good:flac=disabled \
    	-Dgst-plugins-good:dv=disabled \
    	-Dgst-plugins-good:soup=disabled \
    	-Dgst-plugins-bad:gpl=enabled \
    	-Dgst-plugins-bad:va=enabled \
    	-Dgst-plugins-bad:doc=disabled \
    	-Dgst-plugins-bad:nls=disabled \
    	-Dgst-plugins-bad:neon=disabled \
    	-Dgst-plugins-bad:directfb=disabled \
    	-Dgst-plugins-bad:openni2=disabled \
    	-Dgst-plugins-bad:fdkaac=disabled \
    	-Dgst-plugins-bad:ladspa=disabled \
    	-Dgst-plugins-bad:assrender=disabled \
    	-Dgst-plugins-bad:bs2b=disabled \
    	-Dgst-plugins-bad:flite=disabled \
    	-Dgst-plugins-bad:rtmp=disabled \
    	-Dgst-plugins-bad:sbc=disabled \
    	-Dgst-plugins-bad:teletext=disabled \
    	-Dgst-plugins-bad:hls-crypto=openssl \
    	-Dgst-plugins-bad:libde265=enabled \
    	-Dgst-plugins-bad:openh264=enabled \
    	-Dgst-plugins-bad:uvch264=enabled \
    	-Dgst-plugins-bad:x265=enabled \
    	-Dgst-plugins-bad:curl=enabled \
    	-Dgst-plugins-bad:curl-ssh2=enabled \
    	-Dgst-plugins-bad:opus=enabled \
    	-Dgst-plugins-bad:dtls=enabled \
    	-Dgst-plugins-bad:srtp=enabled \
    	-Dgst-plugins-bad:webrtc=enabled \
    	-Dgst-plugins-bad:webrtcdsp=disabled \
    	-Dgst-plugins-bad:dash=disabled \
    	-Dgst-plugins-bad:aja=disabled \
    	-Dgst-plugins-bad:openjpeg=disabled \
    	-Dgst-plugins-bad:analyticsoverlay=disabled \
    	-Dgst-plugins-bad:closedcaption=disabled \
    	-Dgst-plugins-bad:ttml=disabled \
    	-Dgst-plugins-bad:codec2json=disabled \
    	-Dgst-plugins-bad:qroverlay=disabled \
    	-Dgst-plugins-bad:soundtouch=disabled \
    	-Dgst-plugins-bad:isac=disabled \
    	-Dgst-plugins-ugly:nls=disabled \
    	-Dgst-plugins-ugly:x264=enabled \
    	-Dgst-plugins-ugly:gpl=enabled \
    	-Dgstreamer-vaapi:encoders=enabled \
    	-Dgstreamer-vaapi:drm=enabled \
    	-Dgstreamer-vaapi:glx=enabled \
    	-Dgstreamer-vaapi:wayland=enabled \
    	-Dgstreamer-vaapi:egl=enabled \
		--buildtype=release \
		--prefix=/opt/intel/dlstreamer/gstreamer \
		--libdir=lib/ \
		--libexecdir=bin/ \
		build/
	cd build/gstreamer && ninja -C build
	cd build/gstreamer && sudo env PATH=~/python3venv/bin:${PATH} meson install -C build/

.PHONY: opencv
opencv: 
	mkdir -p build
	-cd build && git clone -b ${OPENCV_TAG} https://github.com/opencv/opencv.git
	-cd build && git clone -b ${OPENCV_TAG} https://github.com/opencv/opencv_contrib.git
	mkdir -p build/opencv/build
	cd build/opencv/build && cmake \
		-DBUILD_TESTS=OFF \
    	-DBUILD_PERF_TESTS=OFF \
    	-DBUILD_EXAMPLES=OFF \
    	-DBUILD_opencv_apps=OFF \
    	-DOPENCV_EXTRA_MODULES_PATH=../../opencv_contrib/modules \
    	-GNinja .. && \
		ninja -j ${nproc} && \
		ninja install

.PHONY: build
build: gstreamer ## Compile DLStreamer
	source ${OPENVINO_DIR}/setupvars.sh
	cd build && cmake \
		-DENABLE_PAHO_INSTALLATION=ON \
		-DENABLE_RDKAFKA_INSTALLATION=ON \
		-DENABLE_VAAPI=ON \
		-DENABLE_SAMPLES=ON \
		.. \
		&& \
		make -j "$(nproc)"

.PHONY: build22
build22: opencv build

.PHONY: install
install: build ## Build and install DLStreamer
	@echo "Installing DLStreamer"
	@sudo mkdir -p /opt/intel/
	@sudo cp -r build/intel64/${BUILD_ARG} /opt/intel/dlstreamer/
	@sudo cp -r samples/ /opt/intel/dlstreamer/
	@sudo cp -r python/ /opt/intel/dlstreamer/
	@sudo cp -r scripts/ /opt/intel/dlstreamer/
	@sudo cp -r include/ /opt/intel/dlstreamer/
	@sudo cp README.md /opt/intel/dlstreamer/
	@echo "Installation successful"

.PHONY: deb
deb: ## Build the DLStreamer DEB package for Ubuntu 24.04
	mkdir -p build/packages/deb
	docker build . \
		-f docker/ubuntu/ubuntu24.Dockerfile \
		-t deb-builder \
		--target deb-builder \
		--build-arg http_proxy=${http_proxy} \
		--build-arg https_proxy=${https_proxy} \
		--build-arg DLSTREAMER_VERSION=${DLSTREAMER_VERSION} \
		--build-arg DLSTREAMER_BUILD_NUMBER=1 \
		--build-arg DEV_MODE=true \
		--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY}
	docker create \
		--name deb-builder \
		deb-builder
	docker cp deb-builder:/intel-dlstreamer_${DLSTREAMER_VERSION}.1_amd64.deb ./build/packages/deb
	docker rm deb-builder

.PHONY: deb22
deb22: ## Build the DLStreamer DEB package for Ubuntu 22.04
	mkdir -p build/packages/deb
	docker build . \
		-f docker/ubuntu/ubuntu22.Dockerfile \
		-t deb-builder \
		--target deb-builder \
		--build-arg http_proxy=${http_proxy} \
		--build-arg https_proxy=${https_proxy} \
		--build-arg DLSTREAMER_VERSION=${DLSTREAMER_VERSION} \
		--build-arg DLSTREAMER_BUILD_NUMBER=1 \
		--build-arg DEV_MODE=true \
		--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY}
	docker create \
		--name deb-builder \
		deb-builder
	docker cp deb-builder:/intel-dlstreamer_${DLSTREAMER_VERSION}.1_amd64.deb ./build/packages/deb
	docker rm deb-builder

.PHONY: rpm
rpm: ## Build the DLStreamer RPM package
	mkdir -p build/packages/rpm
	docker build . \
		-t rpm-builder \
		-f docker/fedora41/fedora41.Dockerfile \
		--target rpm-builder \
		--build-arg http_proxy=${http_proxy} \
		--build-arg https_proxy=${https_proxy} \
		--build-arg DLSTREAMER_VERSION=${DLSTREAMER_VERSION} \
		--build-arg DLSTREAMER_BUILD_NUMBER=1 \
		--build-arg DEV_MODE=true \
		--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY}
	docker create \
		--name rpm-builder \
		rpm-builder
	docker cp rpm-builder:/intel-dlstreamer-${DLSTREAMER_VERSION}.1-1.fc41.x86_64.rpm ./build/packages/rpm
	docker rm rpm-builder

.PHONY: image
image: ## Build the DLStreamer docker image based on Ubuntu 24.04
	docker build . \
		-f docker/ubuntu/ubuntu24.Dockerfile \
		-t dlstreamer:dev \
		--target dlstreamer \
		--build-arg http_proxy=${http_proxy} \
		--build-arg https_proxy=${https_proxy} \
		--build-arg DLSTREAMER_VERSION=${DLSTREAMER_VERSION} \
		--build-arg DLSTREAMER_BUILD_NUMBER=1 \
		--build-arg DEV_MODE=true \
		--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY}

.PHONY: image22
image22: ## Build the DLStreamer docker image based on Ubuntu 22.04
	docker build . \
		-f docker/ubuntu/ubuntu22.Dockerfile \
		-t dlstreamer:dev \
		--target dlstreamer \
		--build-arg http_proxy=${http_proxy} \
		--build-arg https_proxy=${https_proxy} \
		--build-arg DLSTREAMER_VERSION=${DLSTREAMER_VERSION} \
		--build-arg DLSTREAMER_BUILD_NUMBER=1 \
		--build-arg DEV_MODE=true \
		--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY}

.PHONY: clean
clean: ## Cleanup any build artifacts
	@rm -rf build

.PHONY: help
help: ## Display help about the commands
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
