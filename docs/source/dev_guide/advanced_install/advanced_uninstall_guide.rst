Ubuntu advanced uninstall guide
===============================

Option #1: Uninstall Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework from APT repository
--------------------------------------------------------------------------------------------------------------


If you installed via apt package just simple uninstall with apt

.. code:: sh

    sudo dpkg-query -l | awk '/^ii/ && /intel-dlstreamer/ {print $2}' | sudo xargs apt-get remove -y --purge


Option #2: Intel® DL Streamer Pipeline Framework Docker image
-------------------------------------------------------------

If you used docker, you need just remove container and dlstreamer docker image

.. code:: sh

    docker rm <container-name>
    docker rmi dlstreamer:devel


Option #3: Compiled version
---------------------------

If you compiled from sources, follow by this instructions

Step 1: Uninstall Intel® DL Streamer
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: sh

    sudo apt-get remove intel-dlstreamer-gst libpython3-dev python-gi-dev libopencv-dev libva-dev


Uninstall Python dependencies

.. code:: sh

    cd ~/intel/dlstreamer_gst
    sudo python3 -m pip uninstall -r requirements.txt 

Step 2: Uninstall optional components
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. note::
  If you haven't installed any optional/additional components, you can skip this step.

Uninstall OpenCL™

.. code:: sh

    sudo apt remove intel-opencl-icd ocl-icd-opencl-dev opencl-clhpp-headers

Uninstall Intel® oneAPI DPC++/C++ Compiler

.. code:: sh

    sudo apt remove intel-oneapi-compiler-dpcpp-cpp intel-level-zero-gpu level-zero-dev

Uninstall VA-API dependencies

.. code:: sh

    sudo apt remove intel-dlstreamer-gst-vaapi libva-dev vainfo intel-media-va-driver-non-free


Step 3: Remove source directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: sh

    rm -rf ~/intel/dlstreamer_gst

-----

.. include:: ../../include/disclaimer_footer.rst

