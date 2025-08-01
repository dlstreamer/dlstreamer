Advanced installation - compilation from source files
============================================================

The easiest way to install Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework is installing it from pre-built Debian packages.
If you would like to follow this way, please go to :doc:`../../get_started/install/install_guide_ubuntu`.

The instruction below focuses on installation steps with building Intel® DL Streamer Pipeline Framework from the source code
provided in `Open Edge Platform repository <https://github.com/open-edge-platform/edge-ai-libraries.git>`__.

Step 1: Install prerequisites (only for Ubuntu)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Please go through prerequisites 1 & 2 described in :doc:`../../get_started/install/install_guide_ubuntu`


Step 2: Install build dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. tabs::

    .. tab:: Ubuntu 24

        .. code:: sh

            sudo apt-get update && \
            sudo apt-get install -y wget vainfo xz-utils python3-pip python3-gi gcc-multilib libglib2.0-dev \
                flex bison autoconf automake libtool libogg-dev make g++ libva-dev yasm libglx-dev libdrm-dev \
                python-gi-dev python3-dev unzip libgflags-dev libcurl4-openssl-dev \
                libgirepository1.0-dev libx265-dev libx264-dev libde265-dev gudev-1.0 libusb-1.0 nasm python3-venv \
                libcairo2-dev libxt-dev libgirepository1.0-dev libgles2-mesa-dev wayland-protocols \
                libssh2-1-dev cmake git valgrind numactl libvpx-dev libopus-dev libsrtp2-dev libxv-dev \
                linux-libc-dev libpmix2t64 libhwloc15 libhwloc-plugins libxcb1-dev libx11-xcb-dev \
                ffmpeg librdkafka-dev libpaho-mqtt-dev libopencv-dev libpostproc-dev libavfilter-dev libavdevice-dev \
                libswscale-dev libswresample-dev libavutil-dev libavformat-dev libavcodec-dev libtbb12 libxml2-dev

    .. tab:: Ubuntu 22

        .. code:: sh

            sudo apt-get update && \
            sudo apt-get install -y wget vainfo xz-utils python3-pip python3-gi gcc-multilib libglib2.0-dev \
                flex bison autoconf automake libtool libogg-dev make g++ libva-dev yasm libglx-dev libdrm-dev \
                python-gi-dev python3-dev unzip libgflags-dev \
                libgirepository1.0-dev libx265-dev libx264-dev libde265-dev gudev-1.0 libusb-1.0 nasm python3-venv \
                libcairo2-dev libxt-dev libgirepository1.0-dev libgles2-mesa-dev wayland-protocols libcurl4-openssl-dev \
                libssh2-1-dev cmake git valgrind numactl libvpx-dev libopus-dev libsrtp2-dev libxv-dev \
                linux-libc-dev libpmix2 libhwloc15 libhwloc-plugins libxcb1-dev libx11-xcb-dev \
                ffmpeg libpaho-mqtt-dev libpostproc-dev libavfilter-dev libavdevice-dev \
                libswscale-dev libswresample-dev libavutil-dev libavformat-dev libavcodec-dev libxml2-dev

    .. tab:: Fedora 41

        .. code:: sh

            sudo dnf install -y \
                https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
                https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

            sudo dnf install -y wget libva-utils xz python3-pip python3-gobject gcc gcc-c++ glibc-devel glib2-devel \
                flex bison autoconf automake libtool libogg-devel make libva-devel yasm mesa-libGL-devel libdrm-devel \
                python3-gobject-devel python3-devel tbb gnupg2 unzip opencv-devel gflags-devel openssl-devel openssl-devel-engine \
                gobject-introspection-devel x265-devel x264-devel libde265-devel libgudev-devel libusb1 libusb1-devel nasm python3-virtualenv \
                cairo-devel cairo-gobject-devel libXt-devel mesa-libGLES-devel wayland-protocols-devel libcurl-devel which \
                libssh2-devel cmake git valgrind numactl libvpx-devel opus-devel libsrtp-devel libXv-devel paho-c-devel \
                kernel-headers pmix pmix-devel hwloc hwloc-libs hwloc-devel libxcb-devel libX11-devel libatomic intel-media-driver

Step 3: Set up a Python environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create a Python virtual environment and install required Python packages:

.. code:: sh

    python3 -m venv ~/python3venv
    source ~/python3venv/bin/activate

    pip install --upgrade pip==24.0
    pip install meson==1.4.1 ninja==1.11.1.1

Step 4: Build/Install FFmpeg 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

NOTE: If you have built and installed a different version of ffmpeg locally, it can cause build errors. It is recommended to uninstall it first.
You can uninstall it with the following command (if installed from source):

.. tabs::

    .. tab:: Ubuntu

        You can uninstall it with the following command (if installed from source):

        .. code:: sh

            cd ${HOME}/ffmpeg # Change to the directory where ffmpeg was built
            sudo make uninstall

        Then reinstall ffmpeg libs:

        .. code:: sh

            sudo apt-get install --reinstall ffmpeg libpostproc-dev libavfilter-dev libavdevice-dev \
                        libswscale-dev libswresample-dev libavutil-dev libavformat-dev libavcodec-dev

    .. tab:: Fedora

        You can uninstall it with the following command (if installed from source):

        .. code:: sh

            cd ${HOME}/ffmpeg # Change to the directory where ffmpeg was built
            sudo make uninstall

        Download and build FFmpeg:

        .. code:: sh

            mkdir ~/ffmpeg
            wget --no-check-certificate https://ffmpeg.org/releases/ffmpeg-6.1.1.tar.gz -O ~/ffmpeg/ffmpeg-6.1.1.tar.gz
            tar -xf ~/ffmpeg/ffmpeg-6.1.1.tar.gz -C ~/ffmpeg
            rm ~/ffmpeg/ffmpeg-6.1.1.tar.gz

            cd ~/ffmpeg/ffmpeg-6.1.1
            ./configure --enable-pic --enable-shared --enable-static --enable-avfilter --enable-vaapi \
                --extra-cflags="-I/include" --extra-ldflags="-L/lib" --extra-libs=-lpthread --extra-libs=-lm --bindir="/bin"
            make -j "$(nproc)"
            sudo make install

Step 5: Build GStreamer
^^^^^^^^^^^^^^^^^^^^^^^

Make sure that previous GStreamer installation is removed:

.. code:: sh

    sudo rm -rf /opt/intel/dlstreamer/gstreamer

Clone and build GStreamer:

.. code:: sh

    cd ~
    git clone https://gitlab.freedesktop.org/gstreamer/gstreamer.git

    cd ~/gstreamer
    git switch -c "1.26.4" "tags/1.26.4"
    export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig/:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
    sudo ldconfig
    meson setup -Dexamples=disabled -Dtests=disabled -Dvaapi=enabled -Dgst-examples=disabled --buildtype=release --prefix=/opt/intel/dlstreamer/gstreamer --libdir=lib/ --libexecdir=bin/ build/
    ninja -C build
    sudo env PATH=~/python3venv/bin:$PATH meson install -C build/

Step 6: Build OpenCV
^^^^^^^^^^^^^^^^^^^^

NOTE: If you have built and installed a different version of OpenCV locally, it can cause build errors. It is recommended to uninstall it first.
You can uninstall it with the following command (if installed from source):

.. code:: sh

    cd ${HOME}/opencv/build # Change to the directory where OpenCV was built
    sudo ninja uninstall
        
.. tabs::

    .. tab:: Ubuntu 24

        After uninstalling OpenCV, reinstall it with the following command:
        
        .. code:: sh

            sudo apt-get install --reinstall libopencv-dev

    .. tab:: Ubuntu 22
        
        NOTE: If you have installed different version of OpenCV using apt-get, you can uninstall it with the command below instead:
        
        .. code:: sh

            sudo apt-get remove --purge libopencv*

        Download and build OpenCV:
        
        .. code:: sh

            wget --no-check-certificate -O ~/opencv.zip https://github.com/opencv/opencv/archive/4.6.0.zip
            wget --no-check-certificate -O ~/opencv_contrib.zip https://github.com/opencv/opencv_contrib/archive/4.6.0.zip
            unzip opencv.zip && \
            unzip opencv_contrib.zip && \
            rm opencv.zip opencv_contrib.zip && \
            mv opencv-4.6.0 opencv && \
            mv opencv_contrib-4.6.0 opencv_contrib && \
            mkdir -p opencv/build

            cd ~/opencv/build
            cmake -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -DOPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules -GNinja ..
            ninja -j "$(nproc)"
            sudo env PATH=~/python3venv/bin:$PATH ninja install

    .. tab:: Fedora 41
         
         NOTE: If you have installed different version of OpenCV using dnf, it is recommended to uninstall it first.
         You can uninstall it with the command below:
        
        .. code:: sh
            sudo dnf remove --allmatches opencv*
        
        Download and build OpenCV:

        .. code:: sh

            wget --no-check-certificate -O ~/opencv.zip https://github.com/opencv/opencv/archive/4.10.0.zip
            wget --no-check-certificate -O ~/opencv_contrib.zip https://github.com/opencv/opencv_contrib/archive/4.10.0.zip
            unzip opencv.zip && \
            unzip opencv_contrib.zip && \
            rm opencv.zip opencv_contrib.zip && \
            mv opencv-4.10.0 opencv && \
            mv opencv_contrib-4.10.0 opencv_contrib && \
            mkdir -p opencv/build

            cd ~/opencv/build
            cmake -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -DOPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules -GNinja ..
            ninja -j "$(nproc)"
            sudo env PATH=~/python3venv/bin:$PATH ninja install

Step 7: Clone Intel® DL Streamer repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: sh

    cd ~
    git clone https://github.com/open-edge-platform/edge-ai-libraries.git
    cd edge-ai-libraries
    git submodule update --init libraries/dl-streamer/thirdparty/spdlog

Step 8: Install OpenVINO™ Toolkit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Download and install OpenVINO™ Toolkit:

.. code:: sh

    cd ~/edge-ai-libraries/libraries/dl-streamer
    sudo ./scripts/install_dependencies/install_openvino.sh

.. note::

    In case of any problems with the installation scripts, `Follow OpenVINO™ Toolkit instruction guide here <https://docs.openvino.ai/2025/get-started/install-openvino/install-openvino-archive-linux.html>`__ to install OpenVINO™ on Linux.

    * Environment: **Runtime**
    * Operating System: **Linux**
    * Version: **Latest**
    * Distribution: **OpenVINO™ Archives**

    After successful OpenVINO™ Toolkit package installation, run the
    following commands to install OpenVINO™ Toolkit dependencies and enable
    OpenVINO™ Toolkit development environment:

    .. code:: sh

        sudo -E /opt/intel/openvino_2025/install_dependencies/install_openvino_dependencies.sh
        source /opt/intel/openvino_2025/setupvars.sh

Step 9: Build Intel DLStreamer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. tabs::

    .. tab:: Ubuntu 24

        .. code:: sh

            cd ~/edge-ai-libraries/libraries/dl-streamer

            mkdir build
            cd build

            export PKG_CONFIG_PATH="/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:${PKG_CONFIG_PATH}"
            source /opt/intel/openvino_2025/setupvars.sh

            cmake -DENABLE_PAHO_INSTALLATION=ON -DENABLE_RDKAFKA_INSTALLATION=ON -DENABLE_VAAPI=ON -DENABLE_SAMPLES=ON ..
            make -j "$(nproc)"
    
    .. tab:: Ubuntu 22

        .. code:: sh

            cd ~/edge-ai-libraries/libraries/dl-streamer

            curl -sSL https://github.com/edenhill/librdkafka/archive/v2.3.0.tar.gz | tar -xz
            cd /librdkafka-2.3.0
            ./configure && make && make install

            mkdir build
            cd build

            export PKG_CONFIG_PATH="/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:${PKG_CONFIG_PATH}"
            source /opt/intel/openvino_2025/setupvars.sh

            cmake -DENABLE_PAHO_INSTALLATION=ON -DENABLE_RDKAFKA_INSTALLATION=ON -DENABLE_VAAPI=ON -DENABLE_SAMPLES=ON ..
            make -j "$(nproc)"
    
    .. tab:: Fedora

        .. code:: sh

            cd ~/edge-ai-libraries/libraries/dl-streamer

            curl -sSL https://github.com/edenhill/librdkafka/archive/v2.3.0.tar.gz | tar -xz
            cd ./librdkafka-2.3.0
            ./configure && make && make INSTALL=install install

            mkdir build
            cd build

            export PKG_CONFIG_PATH="/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:${PKG_CONFIG_PATH}"
            source /opt/intel/openvino_2025/setupvars.sh

            cmake -DENABLE_PAHO_INSTALLATION=ON -DENABLE_RDKAFKA_INSTALLATION=ON -DENABLE_VAAPI=ON -DENABLE_SAMPLES=ON ..
            make -j "$(nproc)"

Step 10: Set up environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set up the required environment variables:

.. tabs::

    .. tab:: Ubuntu

        .. code:: sh

            export LIBVA_DRIVER_NAME=iHD
            export GST_PLUGIN_PATH="$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/usr/lib/x86_64-linux-gnu/gstreamer-1.0"
            export LD_LIBRARY_PATH="/opt/intel/dlstreamer/gstreamer/lib:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"
            export LIBVA_DRIVERS_PATH="/usr/lib/x86_64-linux-gnu/dri"
            export GST_VA_ALL_DRIVERS="1"
            export PATH="/opt/intel/dlstreamer/gstreamer/bin:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/bin:$HOME/.local/bin:$HOME/python3venv/bin:$PATH"
            export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:$PKG_CONFIG_PATH"
            export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

    .. tab:: Fedora

        .. code:: sh

            export LIBVA_DRIVER_NAME=iHD
            export GST_PLUGIN_PATH="$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/usr/lib64/gstreamer-1.0"
            export LD_LIBRARY_PATH="/opt/intel/dlstreamer/gstreamer/lib:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"
            export LIBVA_DRIVERS_PATH="/usr/lib64/dri-nonfree"
            export GST_VA_ALL_DRIVERS="1"
            export PATH="/opt/intel/dlstreamer/gstreamer/bin:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/bin:$HOME/.local/bin:$HOME/python3venv/bin:$PATH"
            export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib/pkgconfig:/usr/lib64/pkgconfig:/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:$PKG_CONFIG_PATH"
            export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX

.. note::

   To set up Linux with the relevant environment variables every time a new terminal is opened, open ~/.bashrc and add the above lines

Step 11: Install Python dependencies (optional)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you intend to use Python elements or samples, you need to install the
necessary dependencies using the following commands:

.. code:: sh

    sudo apt-get install -y -q --no-install-recommends gcc cmake python3-full python-gi-dev python3-dev python3-pip \
        libglib2.0-dev libcairo2-dev libopencv-objdetect-dev libopencv-photo-dev libopencv-stitching-dev libopencv-video-dev \
        libopencv-calib3d-dev libopencv-core-dev libopencv-dnn-dev libgirepository1.0-dev

    source ~/python3venv/bin/activate
    cd ~/edge-ai-libraries/libraries/dl-streamer
    python3 -m pip install -r requirements.txt
