# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

LIBVA_VERSION=382acf177ce18c069e0293408afa34c5875296ff
LIBVAUTILS_VERSION=2.3.0
MEDIADRIVER_VERSION=intel-media-18.4.pre5
GMMLIB_VERSION=intel-gmmlib-18.4.1

git clone -b ${GMMLIB_VERSION} https://github.com/intel/gmmlib.git && mkdir gmmlib/build && cd gmmlib/build && \
       cmake .. && make -j $(($(nproc) + 1)) && sudo make install && cd ../.. && rm -rf gmmlib
git clone https://github.com/intel/libva.git && cd libva && git checkout ${LIBVA_VERSION} && ./autogen.sh  --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu && \
       make -j $(($(nproc) + 1)) && sudo make install && cd .. && rm -rf libva
git clone -b ${LIBVAUTILS_VERSION} https://github.com/intel/libva-utils.git && cd libva-utils && ./autogen.sh && \
       make -j $(($(nproc) + 1)) && sudo make install && cd .. && rm -rf libva-utils
git clone -b ${MEDIADRIVER_VERSION} https://github.com/intel/media-driver.git && mkdir media-driver/build && cd media-driver/build && \
       cmake .. && make -j $(($(nproc) + 1)) && sudo make install && cd ../.. && rm -rf media-driver
