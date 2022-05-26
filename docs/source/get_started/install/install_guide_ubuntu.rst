Install Guide Ubuntu
====================

The easiest way to install Intel® Deep Learning Streamer (Intel® DL Streamer) is installing :ref:`from APT repository <1>`.
If you prefer containerized development or runtime
environment based on Docker, the Intel® DL Streamer is also available in
`2022.1.0-ubuntu20-devel <https://hub.docker.com/r/intel/dlstreamer/dlstreamer:2022.1.0-ubuntu20-devel>`__
or
`2022.1.0-ubuntu20 <https://hub.docker.com/r/intel/dlstreamer/dlstreamer:2022.1.0-ubuntu20>`__
Docker image containers.

Alternatively, you can build Intel® DL Streamer from the source code
provided in `repository <https://github.com/dlstreamer/dlstreamer>`__, either building directly on host system, or
as a Docker image.

The following table summarizes installation options. You can find
detailed instructions for each installation option by following the
links in the first column of the table.


.. list-table::
   :header-rows: 1

   * - Option
     - Supported OS
     - Notes

   * - :ref:`Install Intel® DL Streamer from APT repository <1>`
     - Ubuntu 20.04, Ubuntu 18.04
     - \-
   * - :ref:`Pull and run Intel® DL Streamer Docker image<2>`
     - Any Linux OS as host system
     - Recommended for containerized environment and when host OS is not supported by Intel® DL Streamer installer
   * - :ref:`Compile Intel® DL Streamer from sources on host system <3>`
     - Ubuntu 20.04, Ubuntu 18.04
     - If you want to build Intel® DL Streamer from source code on host system
   * - :ref:`Compile Intel® DL Streamer as Docker image <4>`
     - Any Linux OS as host system
     - If you want to build Docker image from Intel® DL Streamer source code and run as container

Refer to :doc:`Windows Install Guide<install_guide_windows>` for Windows
installation.

Refer to :doc:`Red Hat Enterprise Linux (RHEL) 8 Install Guide<install_guide_rhel>` for RHEL8
installation.

.. _1:

Option #1: Install Intel® DL Streamer from APT repository
--------------------------------------------------------

Step 1: Install required packages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Option a : (Recommended) Specific GPU driver versions used in OpenVINO™ and Intel® DL Streamer validation
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

..  code:: sh

   # Install dependencies
   sudo apt-get update && sudo apt-get install curl gpg-agent software-properties-common

   # Register OpenVINO™ toolkit APT repository
   curl https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add -
   echo "deb https://apt.repos.intel.com/openvino/2022 `. /etc/os-release && echo ${UBUNTU_CODENAME}` main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2022.list

   # Install OpenVINO™ toolkit and recommended version of OpenCL™ driver
   sudo apt-get update && sudo apt-get install openvino-libraries-dev-2022.1.0
   sudo -E /opt/intel/openvino_2022/install_dependencies/install_NEO_OCL_driver.sh

   # Install Intel® DL Streamer and recommended version of media driver
   sudo apt-get update && sudo apt-get install intel-dlstreamer-dev
   sudo -E /opt/intel/dlstreamer/install_dependencies/install_media_driver.sh

   # Setup OpenVINO™ and Intel® DL Streamer environment
   source /opt/intel/openvino_2022/setupvars.sh
   source /opt/intel/dlstreamer/setupvars.sh

Option b : For latest OpenCL™ and media driver versions
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

..  code:: sh

   # Install dependencies
   sudo apt-get update && sudo apt-get install curl gpg-agent software-properties-common

   # Register Intel® Graphics APT repository
   curl -sSL https://repositories.intel.com/graphics/intel-graphics.key | sudo apt-key add  - && \
   . /etc/os-release && sudo -E apt-add-repository "deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu ${UBUNTU_CODENAME} main"

   # Register OpenVINO™ toolkit APT repository
   curl https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add -
   echo "deb https://apt.repos.intel.com/openvino/2022 `. /etc/os-release && echo ${UBUNTU_CODENAME}` main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2022.list

   # Install
   sudo apt-get update && sudo apt-get install intel-dlstreamer-dev

   # Setup OpenVINO™ and Intel® DL Streamer environment
   source /opt/intel/openvino_2022/setupvars.sh
   source /opt/intel/dlstreamer/setupvars.sh


Step 2: Install optional dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to enable all `gvametapublish` backends install required dependencies with scripts:

.. code:: sh

   sudo -E /opt/intel/dlstreamer/install_dependencies/install_mqtt_client.sh
   sudo -E /opt/intel/dlstreamer/install_dependencies/install_kafka_client.sh



To install Intel® DL Streamer package that contains Intel® oneAPI DPC++/C++ Compiler features:

.. code:: sh

   # Register Intel® oneAPI APT repository
   sudo add-apt-repository "deb https://apt.repos.intel.com/oneapi all main"

   # Install
   sudo apt-get update && sudo apt-get install intel-dlstreamer-dpcpp

Also you can install Intel® oneAPI DPC++/C++ Compiler itself if you want:

.. code:: sh

   sudo apt-get install intel-oneapi-compiler-dpcpp-cpp


Available Intel® DL Streamer APT packages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :align: left
   :header-rows: 1

   * - Package name
     - Description

   * - intel-dlstreamer-dev
     - Development package - installs runtime on CPU/GPU, C++/Python bindings, samples, development tools
   * - intel-dlstreamer-cpu
     - Runtime on CPU - files required for execution on CPU
   * - intel-dlstreamer-gpu
     - Runtime on GPU - files required for execution on GPU
   * - intel-dlstreamer
     - Runtime on CPU/GPU - files required for execution on CPU and GPU
   * - intel-dlstreamer-dpcpp
     - Additional runtime on GPU - runtime libraries built on Intel® oneAPI DPC++ Compiler. This package installs DPC++ runtime as dependency
   * - python3-intel-dlstreamer
     - Python runtime - installs Intel® DL Streamer Python bindings and runtime on CPU/GPU
   * - intel-dlstreamer-cpp
     - C++ bindings
   * - intel-dlstreamer-samples
     - C++ and Python samples demonstrating the use of Intel® DL Streamer


.. _2:

Option #2: Pull and run Intel® DL Streamer Docker image
------------------------------------------------------

Step 1: Install Docker
^^^^^^^^^^^^^^^^^^^^^^

`Get Docker <https://docs.docker.com/get-docker/>`__ for your host OS

Step 2: Allow connection to X server
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some Intel® DL Streamer samples use display. Hence, first run the following
commands to allow connection from Docker container to X server on
host:

.. code:: sh

   xhost local:root
   setfacl -m user:1000:r ~/.Xauthority

Step 3: Run Intel® DL Streamer container
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Pull and run Intel® DL Streamer container for development:

.. code:: sh

   docker run -it --privileged --net=host \
      --device /dev/dri \
      -v ~/.Xauthority:/home/dlstreamer/.Xauthority \
      -v /tmp/.X11-unix \
      -e DISPLAY=$DISPLAY \
      -v /dev/bus/usb \
      --rm dlstreamer/dlstreamer:latest-devel /bin/bash

For deployment, you can download Intel® DL Streamer runtime container
with reduced size comparing to development container. Please note
that this container does not have Intel® DL Streamer samples:

.. code:: sh

   docker run -it --privileged --net=host \
      --device /dev/dri \
      -v ~/.Xauthority:/home/dlstreamer/.Xauthority \
      -v /tmp/.X11-unix \
      -e DISPLAY=$DISPLAY \
      -v /dev/bus/usb \
      --rm dlstreamer/dlstreamer:latest /bin/bash

Inside Docker container for development, you can find Intel® DL Streamer
samples in folder
``/opt/intel/dlstreamer/samples``

Before using the Intel® DL Streamer samples, run the script
``download_models.sh`` (located in ``samples`` folder) to download
the models required for samples.

Step 4: Run Intel® DL Streamer container with Intel® oneAPI DPC++/C++ Compiler [OPTIONAL]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To enable execution of ``gvawatermark`` and ``gvatrack``
elements on GPU you should download container with Intel® oneAPI DPC++/C++ Compiler.

.. code:: sh

   docker run -it --privileged --net=host \
      --device /dev/dri \
      -v ~/.Xauthority:/home/dlstreamer/.Xauthority \
      -v /tmp/.X11-unix \
      -e DISPLAY=$DISPLAY \
      -v /dev/bus/usb \
      --rm dlstreamer/dlstreamer:2022.1.0-ubuntu20-dpcpp /bin/bash

.. _gpu-ubuntu20:

Intel® Graphics Compute Runtime for oneAPI Level Zero and OpenCL™ Driver configuration on Ubuntu* 20.04
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Intel® Graphics Compute Runtime for oneAPI Level Zero and OpenCL™ Driver components are required to use a GPU device.
The driver is installed in the Intel® DL Streamer Docker image but you need to activate it in the container for a non-root user if you have Ubuntu 20.04 on your host.

To access GPU capabilities, you need to have correct permissions on the host and Docker container.
Run the following command to list the group assigned ownership of the render nodes on your host:

.. code:: sh

   stat -c "group_name=%G group_id=%g" /dev/dri/render*

Intel® DL Streamer Docker images do not contain a render group for `dlstreamer` non-root user because the render group does not have a strict group ID, unlike the video group.
To run container as non-root with access to a GPU device, specify the render group ID from your host:

.. code:: sh

   docker run -it --device /dev/dri --group-add=<render_group_id_on_host> <other_args> <image_name>

For example, get the render group ID on your host:

.. code:: sh

   docker run -it --device /dev/dri --group-add=$(stat -c "%g" /dev/dri/render*) <other_args> <image_name>

Now you can use the container with GPU access under the non-root user.

.. _3:

Option #3: Compile Intel® DL Streamer from sources on host system
-----------------------------------------------------------------

.. note::
   These instructions are verified on Ubuntu 20.04

Step 1: Clone this repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, clone this repository into folder ``~/intel/dlstreamer_gst``:

.. code:: sh

   mkdir -p ~/intel
   git clone https://github.com/dlstreamer/dlstreamer.git ~/intel/dlstreamer_gst
   cd ~/intel/dlstreamer_gst

Step 2: Install Intel® Distribution of OpenVINO™ Toolkit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Set up the OpenVINO™ Toolkit APT repository:

.. code:: sh

   sudo apt-get update && sudo apt-get install -y --no-install-recommends software-properties-common gnupg curl
   curl https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add -
   echo "deb https://apt.repos.intel.com/openvino/2022 `. /etc/os-release && echo ${UBUNTU_CODENAME}` main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2022.list

Install required packages:

.. code:: sh

   sudo apt-get update && sudo apt-get install -y OpenVINO™ openvino-opencv

After successful OpenVINO™ Toolkit package installation, run the
following commands to install OpenVINO™ Toolkit dependencies and enable
OpenVINO™ Toolkit development environment:

.. code:: sh

   sudo -E /opt/intel/openvino_2022/install_dependencies/install_openvino_dependencies.sh
   source /opt/intel/openvino_2022/setupvars.sh

Install Open Model Zoo tools:

.. code:: sh

   python3 -m pip install --upgrade pip
   python3 -m pip install openvino-dev[onnx]

Step 3: Install Intel® DL Streamer dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Install GStreamer and other build dependencies:

.. code:: sh

   sudo apt-get install -y --no-install-recommends intel-dlstreamer-gst cmake build-essential libpython3-dev python-gi-dev

Step 4: Install Python dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you intend to use Python elements or samples, you need to install the
necessary dependencies using the following commands:

-  If you want to set up a local environment follow this step (skip to
   install all the dependencies globally):

   -  Install the ``virtualenv`` tool to create isolated Python
      environments

      .. code:: sh

         # you can install it within the global python interpreter
         python3 -m pip install virtualenv
         # or you can install it as a user package
         python3 -m pip install --user virtualenv

   -  Create a virtual environment and activate it

      .. code:: sh

         cd ~/intel/dlstreamer_gst
         virtualenv -p /usr/bin/python3 .env3 --system-site-packages
         source .env3/bin/activate

-  Install Python requirements:

   .. code:: sh

      cd ~/intel/dlstreamer_gst
      python3 -m pip install --upgrade pip
      python3 -m pip install -r requirements.txt

.. _install-opencl:

Step 5: Install OpenCL™ [OPTIONAL]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the following commands to install OpenCL™ driver:

.. code:: sh

   # Register Intel® Graphics APT repository:
   curl -sSL https://repositories.intel.com/graphics/intel-graphics.key | sudo apt-key add - && \
   sudo apt-add-repository 'deb [arch=amd64] https://repositories.intel.com/graphics/ubuntu focal main'

   # Install
   sudo apt-get update && sudo apt-get install -y intel-opencl-icd

Step 6: Install message brokers [OPTIONAL]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Installation of message brokers will enable capability of
``gvametapublish`` element to publish inference results to Kafka or
MQTT. Note that ``gvametapublish`` element will be able to publish
inference results to file even if you skip this step.

Run following script to install the message brokers:

.. code:: sh

   cd ~/intel/dlstreamer_gst/scripts/
   sudo ./install_metapublish_dependencies.sh

In order to enable one of these message brokers in Intel® DL Streamer,
corresponding key should be passed to cmake in configuration step. To
enable MQTT please pass ``-DENABLE_PAHO_INSTALLATION=ON`` option to
cmake. To enable Kafka please pass ``-DENABLE_RDKAFKA_INSTALLATION=ON``
option to cmake.

Step 7: Install Intel® oneAPI DPC++/C++ Compiler [OPTIONAL]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:ref:`OpenCL™ driver installation <install-opencl>` is required before continuing.

Intel® oneAPI DPC++ Compiler will enable execution of ``gvawatermark``
and ``gvatrack`` elements on GPU.

.. code:: sh

   # Register Intel® oneAPI APT repository
   curl -sSL https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo apt-key add - && \
   sudo add-apt-repository "deb https://apt.repos.intel.com/oneapi all main"

   # Install Intel® oneAPI DPC++/C++ Compiler
   sudo apt-get update && sudo apt-get install -y intel-oneapi-compiler-dpcpp-cpp intel-level-zero-gpu level-zero-dev

   # Set up environment
   source /opt/intel/oneapi/compiler/latest/env/vars.sh

Step 8: Install VA-API dependencies [OPTIONAL]
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:ref:`OpenCL™ driver installation <install-opencl>` is required before continuing.

To enable VA-API preprocessing of Intel® DL Streamer’s inference
elements run the following commands:

.. code:: sh

   sudo apt-get install -y intel-dlstreamer-gst-vaapi libva-dev vainfo intel-media-va-driver-non-free
   export LIBVA_DRIVER_NAME=iHD
   # Check installation
   vainfo

The output shouldn’t contain any error messages, iHD driver must be
found. If no errors occur, please proceed further to the next step.

If you receive error message like the one below, please reboot your
machine and try again.

.. code:: sh

   libva info: va_openDriver() returns -1
   vaInitialize failed with error code -1 (unknown libva error), exit

If you receive any other errors, please retry installation process. If
it’s not even after re-installation, please submit an issue for
support on GitHub
`here <https://github.com/dlstreamer/dlstreamer/issues>`__.

Additionally, pass ``-DENABLE_VAAPI=ON`` option to cmake in configuration step.

Step 9: Build Intel® DL Streamer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With all dependencies installed, proceed to build Intel® DL Streamer:

.. code:: sh

   # OpenVINO™ Toolkit environment
   source /opt/intel/openvino_2022/setupvars.sh
   # GStreamer environment
   source /opt/intel/dlstreamer/gstreamer/bin/gstreamer-setupvars.sh
   # Intel® oneAPI DPC++/C++ Compiler environment (if installed)
   # source /opt/intel/oneapi/compiler/latest/env/vars.sh

   # cmake
   mkdir ~/intel/dlstreamer_gst/build
   cd ~/intel/dlstreamer_gst/build
   cmake -DCMAKE_INSTALL_PREFIX=/opt/intel/dlstreamer  ..

   # make
   make -j
   sudo make install

   # Setup Intel® DL Streamer environment
   source ~/intel/dlstreamer_gst/scripts/setup_env.sh

Intel® DL Streamer is now ready to use!

You can find samples in folder ``~/intel/dlstreamer_gst/samples``.
Before using the samples, run the script ``download_models.sh`` (located
in ``samples`` folder) to download the models required for samples.

.. note::
   The environment variables are removed when you close the shell.
   As an option, you can set the environment variables in file
   ``~/.bashrc`` for automatic enabling.

.. note::
   The manual installation on the host machine may have environment issues.
   If this happened, please consider other ways we recommend -
   :ref:`building Docker image with Intel® DL Streamer<4>`.
   Also, you can create an issue on GitHub
   `here <https://github.com/dlstreamer/dlstreamer/issues>`__.

.. _4:

Option #4: Compile Intel® DL Streamer as Docker image
----------------------------------------------------

Step 1: Install Docker CE (if not installed)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Refer to `Docker installation
documentation <https://docs.docker.com/install/>`__

Step 2: Clone this repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Clone this repository into folder ``~/intel/dlstreamer_gst``:

.. code:: sh

   mkdir -p ~/intel
   git clone https://github.com/dlstreamer/dlstreamer.git ~/intel/dlstreamer_gst
   cd ~/intel/dlstreamer_gst

Step 3: Build Docker image
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the following command to build Docker image:

.. code:: sh

   cd ~/intel/dlstreamer_gst/docker/source/ubuntu20
   # if you want to build an image with a specific tag then provide it as a second argument (by default latest)
   # if you want to use a Docker registry then provide it as a third argument (in this case you should provide the tag as well)
   ./build_docker_image.sh [tag] [registry]

It’s also possible to build Docker image with ``gvawatermark`` and
``gvatrack`` elements supporting offload on GPU. This is achieved with
Intel® oneAPI DPC++/C++ Compiler installation.

To enable that option, please run the following commands to build Docker
image:

.. code:: sh

   cd ~/intel/dlstreamer_gst/docker/source/ubuntu20

   docker build -f Dockerfile -t dlstreamer:<tag> \
       --build-arg http_proxy=$HTTP_PROXY \
       --build-arg https_proxy=$HTTPS_PROXY \
       --build-arg DOCKER_PRIVATE_REGISTRY=<registry> \
       --build-arg ENABLE_DPCPP_INSTALLATION=ON \
       ~/intel/dlstreamer_gst/

Here is the additional information and the meaning of some options in
the Docker build command:

-  Option ``-f`` is name of the Dockerfile (Default is ‘PATH/Dockerfile’).
-  Option ``-t`` is name and optionally a tag of the Docker image in the ‘name:tag’ format. For example, you may set ``dlstreamer:latest``.
-  Argument ``DOCKER_PRIVATE_REGISTRY`` is needed, if you want to use a Docker registry. Otherwise, you don't need to set this option.
-  Argument ``ENABLE_DPCPP_INSTALLATION`` enables installation of Intel® oneAPI DPC++/C++ Compiler.

The build process may take significant time and should finally create
Docker image ``dlstreamer:<tag>`` (by default ``latest``). Validate
Docker image with command:

.. code:: sh

   docker images | grep dlstreamer

This command will return a line with image ``dlstreamer:<tag>``
description. If the image is absent in your output, please repeat all
the steps above.

Step 4: Run Docker image
^^^^^^^^^^^^^^^^^^^^^^^^

Some Intel® DL Streamer samples use display. Hence, first run the following commands to
allow connection from Docker container to X server on host:

.. code:: sh

   xhost local:root
   setfacl -m user:1000:r ~/.Xauthority

Then, run container:

.. code:: sh

   docker run -it --privileged --net=host \
   \
   -v ~/.Xauthority:/home/dlstreamer/.Xauthority \
   -v /tmp/.X11-unix \
   -e DISPLAY=$DISPLAY \
   -e HTTP_PROXY=$HTTP_PROXY \
   -e HTTPS_PROXY=$HTTPS_PROXY \
   -e http_proxy=$http_proxy \
   -e https_proxy=$https_proxy \
   \
   -v ~/intel/dl_streamer/models:/home/dlstreamer/intel/dl_streamer/models \
   \
   -v ~/intel/dl_streamer/video:/home/dlstreamer/video-examples:ro \
   -e VIDEO_EXAMPLES_DIR=/home/dlstreamer/video-examples \
   \
   dlstreamer:<tag>

.. note::
   If your host OS is Ubuntu 20 refer to :ref:`this guide <gpu-ubuntu20>` in order to setup access to GPU from container.

Here is the additional information and the meaning of some options in
the ``docker run`` command:

-  Option ``--privileged`` is required for Docker container to access the host system’s GPU
-  Option ``--net=host`` provides host network access to container. It is needed for correct interaction with X server.
-  Files ``~/.Xauthority`` and ``/tmp/.X11-unix`` mapped to the container are needed to ensure smooth authentication with X server.
-  ``-v`` instances are needed to map host system directories inside Docker container.
-  ``-e`` instances set Docker container environment variables. The samples need some of them set in order to operate correctly. Proxy variables are needed if the host is behind a firewall.
-  Volume provided for ``models`` folder will be used to download and store the models. Environment variable MODELS_PATH in the Docker container is responsible for it.
-  Entrypoint of the Docker container is by default ``/opt/intel/dlstreamer/samples``.

Inside the Docker image you can find Intel® DL Streamer samples at the entrypoint.
Before using the samples, run the script ``download_models.sh`` (located
in ``samples`` folder) to download the models required for samples.

.. note::
   If you want to use video from web camera as an input in
   sample ``face_detection_and_classification.sh`` you should mount the
   device with this command (add this command when running the container):

   .. code:: sh

      -v /dev/video0:/dev/video0

   Now you can run the sample with video from web camera:

   .. code:: sh

      ./face_detection_and_classification.sh /dev/video0

.. note::
   You can run Docker image using utility script located in ``docker`` folder:

   .. code:: sh

      cd ~/intel/dlstreamer_gst/docker  # essential
      export DATA_PATH=~/intel/dl_streamer  # essential
      sudo ./run_docker_container.sh --video-examples-path=$DATA_PATH/video --models-path=$DATA_PATH/models --image-name=dlstreamer:<tag>

Next Steps
----------

* :doc:`../tutorial`
* `Samples overview <https://github.com/dlstreamer/dlstreamer/blob/master/samples/README.md>`__

-----

.. include:: ../../include/disclaimer_footer.rst

