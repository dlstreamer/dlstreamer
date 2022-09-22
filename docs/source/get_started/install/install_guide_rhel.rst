Install Guide RHEL
===================

For Red Hat Enterprise Linux 8, Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework can be built from the source code provided in this
repository as Docker image.

Compile Intel® DL Streamer Pipeline Framework as Docker image
-----------------------------------------

To be able to create Intel® DL Streamer Pipeline Framework Docker image, **activated RHEL Docker image is required**.

Step 1: Install Docker CE (if not installed)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Refer to `Docker installation documentation <https://docs.docker.com/install/>`__

Step 2: Clone the repository
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Clone this repository into folder ``~/intel/dlstreamer``:

.. code:: sh

   mkdir -p ~/intel
   git clone https://github.com/dlstreamer/dlstreamer.git ~/intel/dlstreamer
   cd ~/intel/dlstreamer

Step 3: Build Docker image
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run the following command to build Docker image:

.. code:: sh

   cd ~/intel/dlstreamer/docker/source/rhel8
   # Provide a name and a tag of activated RHEL Docker image as a first argument.
   # If you want to build an image with a specific tag then provide it
   # as a second argument (by default latest).
   ./build_docker_image.sh <image name> [tag]


The build process may take significant time and should finally create
Docker image ``dlstreamer:<tag>`` (by default ``latest``). Obtained Docker image
can be validated with command:

.. code:: sh

   docker images | grep dlstreamer

This command will return a line with image ``dlstreamer:<tag>``
description. If the image is absent in the output, please repeat all
the steps above.

Step 4: Run Docker image
^^^^^^^^^^^^^^^^^^^^^^^^

Some Pipeline Framework samples use display to render results.
In order to allow connection from Docker container to host X server run the following commands.

.. code:: sh

   xhost local:root
   setfacl -m user:1000:r ~/.Xauthority

Then, run container:

.. code:: sh

   docker run -it --privileged --net=host \
   -v ~/.Xauthority:/home/dlstreamer/.Xauthority \
   -v /tmp/.X11-unix:/tmp/.X11-unix \
   -e DISPLAY=$DISPLAY \
   -e HTTP_PROXY=$HTTP_PROXY \
   -e HTTPS_PROXY=$HTTPS_PROXY \
   -e http_proxy=$http_proxy \
   -e https_proxy=$https_proxy \
   \
   -v ~/intel/dlstreamer/models:/home/intel/dlstreamer/models \
   -v ~/intel/dlstreamer/video:/home/video-examples:ro \
   -e VIDEO_EXAMPLES_DIR=/home/video-examples \
   \
   dlstreamer:<tag>

.. note::
   | If your host OS is Ubuntu 20 add ``--group-add=$(stat -c "%g" /dev/dri/render*)`` argument for ``docker run`` in order to setup access to GPU from container.
   | Refer to :ref:`this guide <gpu-ubuntu20>` for more details.

Here is the additional information and the meaning of some options in
the Docker run command:

- Option ``--privileged`` is required for Docker container to access the host system’s GPU.
- Option ``--net=host`` provides host network access to container. It is needed for correct
  interaction with X server.
- Files ``~/.Xauthority`` and ``/tmp/.X11-unix`` mapped to the container are needed to ensure
  smooth authentication with X server.
- ``-v`` instances are needed to map host system directories inside Docker container.
- ``-e`` instances set Docker container environment variables. The samples need some of them set in order to operate correctly. Proxy variables are needed if the host is behind a firewall.
- Volume provided for ``models`` folder will be used to download and store the models.
  Environment variable MODELS_PATH in the Docker container is responsible for it.
- Entrypoint of the Docker is by default ``/opt/intel/dlstreamer/samples``.


Inside Docker image you can find Pipeline Framework samples at the entrypoint.
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

      cd ~/intel/dlstreamer/docker/source/rhel8  # essential
      export DATA_PATH=~/intel/dlstreamer  # essential
      sudo ./run_docker_container.sh --video-examples-path=$DATA_PATH/video --models-path=$DATA_PATH/models --image-name=dlstreamer:<tag>


Next Steps
----------

* :doc:`../tutorial`
* `Samples overview <https://github.com/dlstreamer/dlstreamer/blob/master/samples/README.md>`__

::

   * Other names and brands may be claimed as the property of others.

