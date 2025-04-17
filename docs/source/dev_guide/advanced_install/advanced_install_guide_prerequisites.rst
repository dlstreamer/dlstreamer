Ubuntu advanced installation - prerequisites
============================================

To use GPU and/or NPU as an inference devices or to use graphics hardware encoding/decoding capabilities, it is required to install appropriate drivers.
The easiest way to install these drivers is to use automated script described in: :doc:`../../get_started/install/install_guide_ubuntu`.
This documentation describes how to do these steps manually.


Prerequisite 1 - Intel® GPU drivers for computing and media runtimes
--------------------------------------------------------------------

To use GPU as an inference device or to use graphics hardware encoding/decoding capabilities, it is required to install GPU computing and media runtime drivers.

A. Install dependencies and register additional APT repositories.

..  code:: sh

   # Install dependencies
   sudo apt-get update && sudo apt-get install curl wget gpg software-properties-common jq

   # Register Intel® oneAPI APT repository
   curl -fsSL https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB | sudo gpg --dearmor --output /usr/share/keyrings/intel-sw-products.gpg
   echo "deb [signed-by=/usr/share/keyrings/intel-sw-products.gpg] https://apt.repos.intel.com/oneapi all main" | sudo tee /etc/apt/sources.list.d/intel-oneapi.list


B. Please cheek that you can see a ``renderD*`` device in ``/dev/dri`` directory before installation. You should be able to see a ``renderD128`` for the integrated GPU or ``renderD129`` for the discrete GPU.

   .. code:: sh

      user@my-host:~$ ll /dev/dri | grep renderD
      crw-rw----  1 root render 226, 128 Aug  6 22:41 renderD128
   
   If you can see see the device, please follow the next step C. If you cannot see it, please follow instructions to the full installation processes, including Kernel Mode drivers:
         -  `For Intel® Data Center GPU Flex Series and Intel® Data Center GPU Max Series <https://dgpu-docs.intel.com/driver/installation.html>`__
         -  `For Intel® Arc™ GPUs <https://dgpu-docs.intel.com/driver/client/overview.html>`__

C. To register Intel® Graphics APT repository, please use **only one** of the following according to your hardware:

-  For Intel® Data Center GPU Flex Series and Intel® Data Center GPU Max Series:
   
   ..  code:: sh
   
      # Download Intel® Graphics APT repository key
      wget -qO - https://repositories.intel.com/graphics/intel-graphics.key | sudo gpg --yes --dearmor --output /usr/share/keyrings/intel-graphics.gpg

      # Download Intel® Graphics APT repository
      # [Ubuntu 24.04]
      echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu noble flex" | sudo tee /etc/apt/sources.list.d/intel-graphics.list
      # [Ubuntu 22.04]
      echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/graphics/ubuntu jammy flex" | sudo tee /etc/apt/sources.list.d/intel-graphics.list


-  For Intel® Client and Arc™ GPUs:
   
   ..  code:: sh
   
       # Download Intel® Graphics APT repository key
      wget -qO - https://repositories.intel.com/gpu/intel-graphics.key | sudo gpg --yes --dearmor --output /usr/share/keyrings/intel-graphics.gpg

      # Download Intel® Graphics APT repository
      # [Ubuntu 24.04]
      echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu noble client" | sudo tee /etc/apt/sources.list.d/intel-graphics.list
      # [Ubuntu 22.04]
      echo "deb [arch=amd64 signed-by=/usr/share/keyrings/intel-graphics.gpg] https://repositories.intel.com/gpu/ubuntu jammy client" | sudo tee /etc/apt/sources.list.d/intel-graphics.list

D. Install or update packages responsible for computing and media runtimes:

   .. code:: sh

      # Install
      sudo apt-get update && sudo apt-get install intel-level-zero-gpu level-zero


Prerequisite 2 - Install Intel® NPU drivers
-------------------------------------------

.. note::
   Optional step for Intel® Core™ Ultra processors

If you want to use NPU AI accelerator, you need to have Intel® NPU drivers installed.

A. Before installation, please make sure that intel_vpu.ko module is enabled on your host:

.. code:: sh

   user@your-host:~$ lsmod | grep intel_vpu
   intel_vpu             245760  0

B. Installing the driver requires the device to be recognized by your system - Kernel Mode driver should be available. It means you can see a ``accel`` device in ``/dev/dri`` directory. If you don't see it, please reboot the host.

   .. code:: sh

      user@my-host:~$ ll /dev/accel/ | grep accel
      crw-rw----  1 root render 261, 0   Aug  6 22:41 accel0

C. Install the newest Intel® NPU driver. Please follow 'Installation procedure' for the newest available driver version described in: https://github.com/intel/linux-npu-driver/releases

.. note::
   If you are experiencing issues with installation process, check all notes and tips in the release note for the newest Intel® NPU driver version: https://github.com/intel/linux-npu-driver/releases.
   Please pay attention to access to the device as a non-root user.

.. note::
   The following error can be reported when running Intel® DL Streamer on NPU device:

   .. code:: sh

      Setting pipeline to PLAYING ...
      New clock: GstSystemClock
      Caught SIGSEGV
      Spinning.

   In such case, please use the following setting as a temporary workaround:

   .. code:: sh

      export ZE_ENABLE_ALT_DRIVERS=libze_intel_vpu.so

   The issue should be fixed with newer versions of Intel® NPU drivers and Intel® OpenVINO™ NPU plugins.


-----

.. include:: ../../include/disclaimer_footer.rst

