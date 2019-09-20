# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

sudo -E apt-get install libpciaccess-dev autoconf libtool

#unset LIBVA_DRIVERS_PATH
#unset LIBVA_DRIVER_NAME

#git clone http://anongit.freedesktop.org/git/mesa/drm.git
wget https://dri.freedesktop.org/libdrm/libdrm-2.4.94.tar.bz2
tar -xvjf libdrm-2.4.94.tar.bz2
cd libdrm-2.4.94
./configure && make && sudo make install
cd ..

git clone https://github.com/intel/libva.git
cd libva
./autogen.sh && make && sudo make install
cd ..

git clone  https://github.com/intel/libva-utils.git
cd libva-utils
./autogen.sh && make && sudo make install
cd ..
