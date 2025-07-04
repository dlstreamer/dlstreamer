Install Guide Ubuntu
====================

The easiest way to install Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework is installation :ref:`from Debian packages using APT repository <2>`.
If you prefer containerized environment based on Docker, the Intel® DL Streamer Pipeline Framework :ref:`Docker image <3>` is available as well as Dockerfile to build runtime Docker image.
Regardless of chosen installations process, please follow :ref:`prerequisites <1>`.


For detailed description of installation process, including the option with building Intel® DL Streamer Pipeline Framework from the source code
provided in `Open Edge Platform repository <https://github.com/open-edge-platform/edge-ai-libraries.git>`__, please follow instructions in: :doc:`../../dev_guide/advanced_install/advanced_install_guide_index`

.. _1:

Prerequisites
-------------

To use GPU and/or NPU as an inference devices or to use graphics hardware encoding/decoding capabilities, it is required to install appropriate drivers.
Please use the script below to detect available device(s) and install these drivers. Please also pay attention to displayed information while the script
has references to other Intel® resources when additional configuration is required.


Step 1: Download the prerequisites installation script
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

..  code:: sh

   mkdir -p ~/intel/dlstreamer_gst
   cd ~/intel/dlstreamer_gst/
   wget -O DLS_install_prerequisites.sh https://raw.githubusercontent.com/open-edge-platform/edge-ai-libraries/main/libraries/dl-streamer/scripts/DLS_install_prerequisites.sh && chmod +x DLS_install_prerequisites.sh


Step 2: Execute the script and follow its instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

..  code:: sh

   ./DLS_install_prerequisites.sh


The script installs all the essential packages needed for most Intel® Client GPU users, including the following packages:

..  code:: sh

   GPU:
      libze-intel-gpu1
      libze1
      intel-opencl-icd
      clinfo
      intel-gsc
   Media:
      intel-media-va-driver-non-free
   NPU:
      intel-driver-compiler-npu
      intel-fw-npu
      intel-level-zero-npu
      level-zero

More details about the packages can be found in the following driver links respectively:
`Intel® Client GPU <https://dgpu-docs.intel.com/driver/client/overview.html#installing-gpu-packages>`__,
`Media <https://github.com/intel/media-driver/releases>`__,
`NPU <https://github.com/intel/linux-npu-driver/releases/tag/v1.13.0>`__.

Running DL Streamer on Intel® Data Center GPU (Flex) requires specific drivers. In such case, please follow drivers installing instruction on `Intel® Data Center GPU website <https://dgpu-docs.intel.com/driver/installation.html#installing-data-center-gpu-lts-releases>`__.

.. _2:

Option #1: Install Intel® DL Streamer Pipeline Framework from Debian packages using APT repository
--------------------------------------------------------------------------------------------------

This option provides the simplest installation flow using apt-get install command.


Step 1: Install prerequisites
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the script DLS_install_prerequisites.sh to install required GPU/NPU drivers.
For more details see :ref:`prerequisites <1>`.

..  code:: sh

   ./DLS_install_prerequisites.sh


Step 2: Setup repositories
^^^^^^^^^^^^^^^^^^^^^^^^^^

..  tabs::

   ..  tab:: Ubuntu 22

      .. code-block:: sh

          sudo -E wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
          sudo wget -O- https://eci.intel.com/sed-repos/gpg-keys/GPG-PUB-KEY-INTEL-SED.gpg | sudo tee /usr/share/keyrings/sed-archive-keyring.gpg > /dev/null
          sudo echo "deb [signed-by=/usr/share/keyrings/sed-archive-keyring.gpg] https://eci.intel.com/sed-repos/$(source /etc/os-release && echo $VERSION_CODENAME) sed main" | sudo tee /etc/apt/sources.list.d/sed.list
          sudo bash -c 'echo -e "Package: *\nPin: origin eci.intel.com\nPin-Priority: 1000" > /etc/apt/preferences.d/sed'
          sudo bash -c 'echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/openvino/2025 ubuntu22 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2025.list'

   ..  tab:: Ubuntu 24

      .. code-block:: sh

          sudo -E wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | gpg --dearmor | sudo tee /usr/share/keyrings/oneapi-archive-keyring.gpg > /dev/null
          sudo wget -O- https://eci.intel.com/sed-repos/gpg-keys/GPG-PUB-KEY-INTEL-SED.gpg | sudo tee /usr/share/keyrings/sed-archive-keyring.gpg > /dev/null
          sudo echo "deb [signed-by=/usr/share/keyrings/sed-archive-keyring.gpg] https://eci.intel.com/sed-repos/$(source /etc/os-release && echo $VERSION_CODENAME) sed main" | sudo tee /etc/apt/sources.list.d/sed.list
          sudo bash -c 'echo -e "Package: *\nPin: origin eci.intel.com\nPin-Priority: 1000" > /etc/apt/preferences.d/sed'
          sudo bash -c 'echo "deb [signed-by=/usr/share/keyrings/oneapi-archive-keyring.gpg] https://apt.repos.intel.com/openvino/2025 ubuntu24 main" | sudo tee /etc/apt/sources.list.d/intel-openvino-2025.list'


.. note::

   If you have OpenVINO™ installed in a version different from 2025.0.0, please uninstall the OpenVINO™ packages using the following commands.

.. code:: sh

   sudo apt remove -y openvino* libopenvino-* python3-openvino*
   sudo apt-get autoremove


Step 3: Install Intel® DL Streamer Pipeline Framework
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::

   This step will also install the required dependencies, including OpenVINO™ Toolkit and GStreamer.


.. code:: sh

   sudo apt update
   sudo apt-get install intel-dlstreamer


**Congratulations! Intel® DL Streamer is now installed and ready for use!**


To see the full list of installed components check the `Dockerfile content for Ubuntu 24 <https://github.com/open-edge-platform/edge-ai-libraries/blob/main/libraries/dl-streamer/docker/ubuntu/ubuntu24.Dockerfile>`__


[Optional] Step 4: Python dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Python packages required to run Intel® DL Streamer python elements or samples are not installed by default.
You can install them using commands from `Advanced Install Guide Compilation / Install Python dependencies <https://dlstreamer.github.io/dev_guide/advanced_install/advanced_install_guide_compilation.html#step-6-install-python-dependencies>`__


[Optional] Step 5: Post installation steps
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Download model and run hello_dlstreamer script**

Before executing any scripts, ensure you have set the MODELS_PATH environment variable to the directory where the model will be downloaded or where it already exists.
The hello_dlstreamer.sh script assumes the availability of the YOLO11s model. If you do not have it, download it using the following command:

.. note::

   The **download_public_models.sh** script will download the YOLO11s model from the Ultralytics website along with other required components and convert it to the OpenVINO™ format.

   If you add the **coco128** argument to the script, the downloaded model will also be quantized to the INT8 precision.

   If you already have the model, skip this step and simply export the MODELS_PATH and execute the **hello_dlstreamer.sh** script.


..  code:: sh

   mkdir -p /home/${USER}/models
   export MODELS_PATH=/home/${USER}/models
   /opt/intel/dlstreamer/samples/download_public_models.sh yolo11s coco128


The **hello_dlstreamer.sh** will set up the required environment variables and runs a sample pipeline to confirm that Intel® DL Streamer is installed correctly.
To run the hello_dlstreamer script, execute the following command:


..  code:: sh

   /opt/intel/dlstreamer/scripts/hello_dlstreamer.sh


.. note::

   To set up Linux with the relevant environment variables every time a new terminal is opened, open ~/.bashrc and add the following lines:

..  code:: sh

   export LIBVA_DRIVER_NAME=iHD
   export GST_PLUGIN_PATH=/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/opt/intel/dlstreamer/gstreamer/lib/:
   export LD_LIBRARY_PATH=/opt/intel/dlstreamer/gstreamer/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/intel/dlstreamer/lib/gstreamer-1.0:/usr/lib:/opt/intel/dlstreamer/build/intel64/Release/lib:/opt/opencv:/opt/openh264:/opt/rdkafka:/opt/ffmpeg:/usr/local/lib/gstreamer-1.0:/usr/local/lib
   export LIBVA_DRIVERS_PATH=/usr/lib/x86_64-linux-gnu/dri
   export GST_VA_ALL_DRIVERS=1
   export PATH=/opt/intel/dlstreamer/gstreamer/bin:/opt/intel/dlstreamer/build/intel64/Release/bin:$PATH
   export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX


or run:

..  code:: sh

   source /opt/intel/dlstreamer/scripts/setup_dls_config.sh

to configure environment variables for the current terminal session.


[Optional] Step 6: Auxiliary installation steps
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**A. Check for installed packages and versions**

.. code:: sh

   apt list --installed | grep intel-dlstreamer


**B. To install a specific version run the following command:**

.. code:: sh

   sudo apt install intel-dlstreamer=<VERSION>.<UPDATE>.<PATCH>


For example

.. code:: sh

   sudo apt install intel-dlstreamer=2025.0.0


**C. List available versions**

.. code:: sh

   sudo apt show -a intel-dlstreamer


.. _3:

Option #2: Install Docker image from Docker Hub and run it
----------------------------------------------------------

Step 1: Install prerequisites
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the script DLS_install_prerequisites.sh to setup your environment.
For more details see :ref:`prerequisites <1>`.

..  code:: sh

   ./DLS_install_prerequisites.sh

Step 2: Install Docker
^^^^^^^^^^^^^^^^^^^^^^

`Get Docker <https://docs.docker.com/get-docker/>`__ for your host OS
 To prevent file permission issues please follow 'Manage Docker as a non-root user' section steps
 described here <https://docs.docker.com/engine/install/linux-postinstall/>

Step 3: Allow connection to X server
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some Pipeline Framework samples use display. Hence, first run the following
commands to allow connection from Docker container to X server on host:

.. code:: sh

   xhost local:root
   setfacl -m user:1000:r ~/.Xauthority

.. note::

   If you want to build Docker image from DLStreamer Dockerfiles please follow steps from: :doc:`../../dev_guide/advanced_install/advanced_build_docker_image`.

Step 4: Pull the Intel® DL Streamer Docker image from Docker Hub
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Visit <https://hub.docker.com/r/intel/dlstreamer/> Intel® DL Streamer image docker hub to select the most appropriate version.
By default , the latest docker image points to Ubuntu 24 version.

For **Ubuntu 22.04** please specify tag e.g. **2025.0.1.2-ubuntu22**.
For **Ubuntu 24.04** please use **latest** tag or specified version e.g. **2025.0.1.2-ubuntu24**.

..  tabs::

   ..  tab:: Ubuntu 22

      .. code-block:: sh

          docker pull intel/dlstreamer:2025.0.1.2-ubuntu22

   ..  tab:: Ubuntu 24

      .. code-block:: sh

          docker pull intel/dlstreamer:latest


Step 5: Run Intel® DL Streamer Pipeline Framework container
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To confirm that your installation is completed successfully, please run a container

..  tabs::

   ..  tab:: Ubuntu 22

      .. code-block:: sh

          docker run -it intel/dlstreamer:2025.0.1.2-ubuntu22

   ..  tab:: Ubuntu 24

      .. code-block:: sh

          docker run -it intel/dlstreamer:latest

In the container, please run the ``gst-inspect-1.0 gvadetect`` to confirm that GStreamer and Intel® DL Streamer are running

.. code:: sh

   gst-inspect-1.0 gvadetect

If your can see the documentation of ``gvadetect`` element, the installation process is completed.

.. image:: gvadetect_sample_help.png


Next Steps
----------

You are ready to use Intel® DL Streamer. For further instructions to run sample pipeline(s), please go to: :doc:`../tutorial`


-----

.. include:: ../../include/disclaimer_footer.rst
