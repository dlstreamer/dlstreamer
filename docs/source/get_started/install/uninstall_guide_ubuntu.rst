Uninstall Guide Ubuntu
======================

Option #1: Uninstall Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework from APT repository
--------------------------------------------------------------------------------------------------------------


If you installed via apt package just simple uninstall with apt

.. code:: sh

    sudo apt remove intel-dlstreamer


Option #2: Intel® DL Streamer Pipeline Framework Docker image
-------------------------------------------------------------

If you used docker, you need just remove container and dlstreamer docker image

.. code:: sh

    docker rm <container-name>
    docker rmi dlstreamer:devel

-----

.. include:: ../../include/disclaimer_footer.rst
