Install Guide Windows (Preview)
===============================

Building Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework from the source code provided in this
repository may be performed either on host system or as a Docker image.

Option 1: Compile Intel DL Streamer Pipeline Framework from sources on host system
----------------------------------------------------------------------------------

.. note::
   These instructions are verified on Windows 10 Version 1909 and Windows 11 Version 10.0.22000.

Step 1: Clone this repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, clone this repository into folder
``%USERPROFILE%\intel\dlstreamer_gst``:

.. code:: bat

   md %USERPROFILE%\intel
   git clone https://github.com/dlstreamer/dlstreamer.git %USERPROFILE%\intel\dlstreamer_gst
   cd %USERPROFILE%\intel\dlstreamer_gst

Step 2: Install dependencies
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Install Python 3.7 for Windows: https://www.python.org/ftp/python/3.7.9/python-3.7.9-amd64.exe

Install CMake 3.19 for Windows: https://github.com/Kitware/CMake/releases/download/v3.19.4/cmake-3.19.4-win64-x64.msi

Install Visual Studio 2019\* with C++ build tools:
https://aka.ms/vs/16/release/vs_buildtools.exe

Install pkg-config using one of the options:

* **Option 1** Downloading binaries:

  * Download the following archives and extract them into the same folder (e.g. ``%USERPROFILE%\pkg-config``):

    * https://download.gnome.org/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip
    * https://download.gnome.org/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip
    * https://ftp.acc.umu.se/pub/GNOME/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip

  * Add path to ``bin`` folder into ``PATH`` env variable

    .. code:: bat

       set PATH=%PATH%;%USERPROFILE%\pkg-config\bin

* **Option 2** Using MSYS2:

  * Download and install MSYS2 from official site: https://www.msys2.org/
  * Using MSYS2 terminal install pkg-config

    .. code:: bat

       pacman -Syu
       pacman -Su
       pacman -S mingw-w64-x86_64-pkg-config

  * Close MSYS2 terminal and open Command Prompt
  * Add path to pkg-config into ``PATH`` env variable

    .. code:: bat

       set PATH=%PATH%;C:\msys64\mingw64\bin

Install GStreamer 1.18.4 runtime and development packages:

* https://gstreamer.freedesktop.org/data/pkg/windows/1.18.4/msvc/gstreamer-1.0-msvc-x86_64-1.18.4.msi
* https://gstreamer.freedesktop.org/data/pkg/windows/1.18.4/msvc/gstreamer-1.0-devel-msvc-x86_64-1.18.4.msi

.. note::
   If you want to use web camera as a video source then mark '*Gstreamer 1.0 plugins for capture*' for installation


Setup environment variables for GStreamer:

.. code:: bat

   set PATH=%PATH%;C:\gstreamer\1.0\msvc_x86_64\bin
   set PKG_CONFIG_PATH=%PKG_CONFIG_PATH%;C:\gstreamer\1.0\msvc_x86_64\lib\pkgconfig

Step 3: Download and install Intel® Distribution of OpenVINO™ Toolkit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Please register to download the latest version of `OpenVINO™ Toolkit for
Windows\* <https://software.seek.intel.com/openvino-toolkit>`__.

For the installation recommended to follow the `OpenVINO™ Toolkit
Installation
Guide <https://docs.openvino.ai/latest/openvino_docs_install_guides_installing_openvino_windows.html>`__.

After successful OpenVINO™ Toolkit package installation, run the
following commands to install OpenVINO™ Toolkit dependencies and enable
OpenVINO™ Toolkit development environment:

.. code:: bat

   @REM Install OpenCV
   powershell.exe -ExecutionPolicy Bypass -File "C:\Program Files (x86)\Intel\openvino_2022\extras\scripts\download_opencv.ps1"

   @REM Install Open Model Zoo tools
   pip install openvino-dev[onnx]

   @REM Setup environment
   "C:\Program Files (x86)\Intel\openvino_2022\setupvars.bat"

Step 4: Build Intel® DL Streamer Pipeline Framework
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

With all dependencies installed, proceed to build Pipeline Framework:

.. code:: bat

   @REM OpenVINO™ Toolkit environment
   "C:\Program Files (x86)\Intel\openvino_2022\setupvars.bat"

   @REM cmake
   md %USERPROFILE%\intel\dlstreamer_gst\build
   cd %USERPROFILE%\intel\dlstreamer_gst\build
   cmake ..

   @REM build
   cmake --build . --config Release

   @REM Setup env variables with script
   %USERPROFILE%\intel\dlstreamer_gst\scripts\setup_env.bat
   
   @REM Verify GStreamer and Intel® DL Streamer Pipeline Framework plugins are now available
   gst-inspect-1.0 gvadetect

Pipeline Framework is now ready to use!

You can find samples in folder
``%USERPROFILE%\intel\dlstreamer_gst\samples``. Before using the
samples, run the script ``download_models.bat`` (located in ``samples``
folder) to download the models required for samples.

.. note::
   The environment variables are removed when you close the
   command line. As an option, you can set them in system "Environment
   Variables" settings.

Option 2: Compile Intel® DL Streamer Pipeline Framework as Docker image
---------------------------------------------------

Step 1: Install Docker Desktop (if not installed)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Refer to `Docker installation
documentation <https://docs.docker.com/install/>`__

.. note::
   Default setup uses WSL2 for Docker daemon. To support Windows
   build steps below, choose ``Switch to Windows containers...``
   from the Docker Desktop menu in the system tray.

Step 2: Clone this repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Clone this repository into folder
``%USERPROFILE%\intel\dlstreamer_gst``:

.. code:: bat

   md %USERPROFILE%\intel
   git clone https://github.com/dlstreamer/dlstreamer.git %USERPROFILE%\intel\dlstreamer_gst
   cd %USERPROFILE%\intel\dlstreamer_gst

Step 3: Build Docker image
^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the following command to build Docker image:

.. code:: bat

   cd %USERPROFILE%\intel\dlstreamer_gst\docker\source\windows
   @REM provide a link to the OpenVINO™ Toolkit development package as a first argument (by default link to 2022.1 package)
   @REM provide a specific image tag (by default latest)
   @REM if you want to use a specific Windows image then provide its full name (by default mcr.microsoft.com/windows/servercore/iis)
   build_docker_image.bat <link> [tag] [base image]

.. note::
   You can provide a link to OpenVINO™ Toolkit development package from `public storage <https://storage.openvinotoolkit.org/repositories/openvino/packages/>`__.
   For example, `w_openvino_toolkit_dev_p_2022.1.0.643.zip <https://storage.openvinotoolkit.org/repositories/openvino/packages/2022.1/>`__.

.. note::
   If you are behind a corporate proxy you may need to update Docker
   configuration and Powershell steps in the Dockerfile.
   For specific guidance on vs_buildtools.exe, refer to 
   `Visual Studio Build Tools <https://docs.microsoft.com/en-us/visualstudio/install/build-tools-container?view=vs-2019>`__.

The build process may take significant time and should finally create
Docker image ``dlstreamer:<tag>`` (by default ``latest``). Validate
Docker image with command:

.. code:: bat

   docker images | findstr dlstreamer

This command will return a line with image ``dlstreamer:<tag>``
description. If the image is absent in your output, please repeat all
the steps above.

Step 4: Run Docker image
^^^^^^^^^^^^^^^^^^^^^^^^

Run container:

.. code:: bat

   docker run -it ^
   -v %USERPROFILE%\intel\dl_streamer\models:C:\intel\dl_streamer\models ^
   -e MODELS_PATH=C:\intel\dl_streamer\models ^
   dlstreamer:<tag>

Here is the additional information and the meaning of some options in the Docker run command:

* ``-v`` instances are needed to map host system directories inside Docker container.
* ``-e`` instances set Docker container environment variables. The samples need some of them set in order to operate correctly.
* Volume provided for ``models`` folder will be used to download and store the models.
  Environment variable ``MODELS_PATH`` is responsible for it.

Container will start with development environment for Visual Studio\*
and OpenVINO™ Toolkit and open PowerShell\* in ``samples`` folder.

Limitations
-----------

-  ``gvatrack`` and ``gvapython`` elements are not available
-  ``gvametapublish`` works only with filesystem and console output
-  Docker image contains only basic GStreamer plugins. Use
   ``gst-inspect-1.0`` command for a full list of available plugins.
-  Refer to `OpenVINO™ Toolkit for
   Windows\* <https://docs.openvino.ai/latest/openvino_docs_install_guides_installing_openvino_docker_windows.html>`__
   for requirements to run inference on GPU in Docker

Next Steps
----------

* :doc:`../tutorial`
* `Samples overview <https://github.com/dlstreamer/dlstreamer/blob/master/samples/README.md>`__

-----

.. include:: ../../include/disclaimer_footer.rst
