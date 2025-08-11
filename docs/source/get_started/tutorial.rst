Tutorial
========

In this tutorial, you will learn how to build video analytics pipelines
using Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework.

-  `About GStreamer <#about-gstreamer>`__
-  `Introduction to Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework <#introduction-to-intel-deep-learning-streamer-intel-dl-streamer-pipeline-framework>`__
-  `Non-Docker tutorial setup <#non-docker-tutorial-setup>`__
-  `Docker tutorial setup <#docker-tutorial-setup>`__
-  :ref:`Exercise 1 - Build object detection pipeline<object_detection>`
-  :ref:`Exercise 2 - Build object classification pipeline<object-classification>`
-  :ref:`Exercise 3 - Use object tracking to improve performance<object-tracking>`
-  :ref:`Exercise 4 - Publish the inference results to a \`.json\` file<result-publishing>`

About GStreamer
---------------

In this section we introduce basic GStreamer\* concepts that you will
use in the rest of the tutorial. If you are already familiar with
GStreamer feel free to skip ahead to the next section - `Introduction to
Intel® DL Streamer Pipeline Framework <#introduction-to-intel-deep-learning-streamer-intel-dl-streamer-pipeline-framework>`__.

`GStreamer <https://gstreamer.freedesktop.org/>`__ is a flexible, fast
and multiplatform open-source multimedia framework. It has an easy to
use command line tool for running pipelines, as well as an API with
bindings in C\*, Python\*, JavaScript\* and more. In this tutorial we will use
the GStreamer command line tool gst-launch-1.0. For more information and
examples please refer to the online documentation for
`gst-launch-1.0 <https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html?gi-language=c>`__.

Pipelines
~~~~~~~~~

The command line tool **gst-launch-1.0** enables developers to describe
a media analytics pipeline as a series of connected elements. The list of
elements, their configuration properties, and their connections are all
specified as a list of strings separated by exclamation marks (!).
**gst-launch-1.0** parses the string and instantiates the software modules
which perform the individual media analytics operations. Internally, the
GStreamer library constructs a pipeline object that contains the
individual elements and handles common operations such as clocking,
messaging, and state changes.

Example with test video input:

.. code:: sh

   gst-launch-1.0 videotestsrc ! ximagesink


Elements
~~~~~~~~

An
`element <https://gstreamer.freedesktop.org/documentation/application-development/basics/elements.html?gi-language=c>`__
is the fundamental building block of a pipeline. Elements perform
specific operations on incoming frames and then push the resulting
frames downstream for further processing. Elements are linked together
textually by exclamation marks (!) with the full chain of elements
representing the entire pipeline. Each element will take data from its
upstream element, process it and then output the data for processing by
the next element.

Elements designated as source elements provide input into the pipeline
from external sources. In this tutorial we use the
`filesrc <https://gstreamer.freedesktop.org/documentation/coreelements/filesrc.html?gi-language=c#filesrc>`__
element that reads input from a local file.

Elements designated as sink elements represent the final stage of a
pipeline. As an example, a sink element could write transcoded frames to
a file on the local disk or open a window to render the video content to
the screen or even restream the content via RTSP. We will use the
standard
`autovideosink <https://gstreamer.freedesktop.org/documentation/autodetect/autovideosink.html?gi-language=c>`__
element to render the video frames on a local display.

We will also use the
`decodebin3 <https://gstreamer.freedesktop.org/documentation/playback/decodebin3.html>`__
utility element. The **decodebin3** element constructs a concrete set of
decode operations based on the given input format and the decoder and
demuxer elements available in the system. At a high level, the **decodebin3**
abstracts the individual operations required to take encoded frames and
produce raw video frames suitable for image transformation and
inferencing.

Properties
~~~~~~~~~~

Elements are configured using key-value pairs called properties. As an
example, the filesrc element has a property named location which
specifies the file path for input.

Example of filesrc element with its filesrc property:

.. code:: sh

   filesrc location=cars_1900.mp4

The documentation for each element, which can be viewed using the
command line tool **gst-inspect-1.0**, describes its properties as well as
the valid range of values for each property.

Introduction to Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework
-------------------------------------------------------------------------------------

Intel® DL Streamer Pipeline Framework is an easy way to construct media analytics
pipelines using Intel® Distribution of OpenVINO™ toolkit. It leverages
the open source media framework GStreamer to provide optimized media operations and
`Deep Learning Inference Engine <https://docs.openvino.ai/2025/index.html>`__
from OpenVINO™ Toolkit to provide optimized inference.

The elements packaged in the Intel® DL Streamer Pipeline Framework binary release can be divided into three categories:

-  Elements for optimized streaming media operations (usb and ip camera
   support, file handling, decode, color-space-conversion, scaling,
   encoding, rendering, etc.). These elements are developed by the larger
   GStreamer community.
-  Elements that use the Deep Learning Inference
   Engine from OpenVINO™ Toolkit or OpenCV for optimized video analytics
   (detection, classification, tracking). These elements are provided as
   part of the Pipeline Framework’s GVA plugin.
-  Elements that convert and
   publish inference results to the screen as overlaid bounding boxes, to a
   file as a list of JSON Objects, or to popular message brokers (Kafka or
   MQTT) as JSON messages. These elements are provided as part of the DL
   Streamer’s GVA plugin.

The elements in the last two categories above are part of Pipeline Framework’s
GVA plugin and start with the prefix ‘gva’. We will describe the ‘gva’
elements used in this tutorial with some important properties here.
Refer to :doc:`Intel® DL Streamer elements <../elements/elements>` page for more details.

-  :doc:`gvadetect <../elements/gvadetect>`
   - Runs detection with the Inference Engine from OpenVINO™ Toolkit. We
   will use it to detect vehicles in a frame and output their bounding
   boxes aka Regions of Interest (ROI).
   ``queue`` element must be put directly after ``gvadetect`` element in pipeline.

   -  model - path to the inference model network file
   -  device - device to run inferencing on
   -  inference-interval - interval between inference requests, the
      bigger the value, the better the throughput. i.e. setting this
      property to 1 would mean run detection on every frame while
      setting it to 5 would run detection on every fifth frame.

-  :doc:`gvaclassify <../elements/gvaclassify>`
   - Runs classification with the Inference Engine from OpenVINO™
   Toolkit. We will use it to label the bounding boxes that gvadetect
   outputs, with the type and color of the vehicle.
   ``queue`` element must be put directly after ``gvaclassify`` element in pipeline.

   -  model - path to the inference model network file
   -  model-proc - path to the model-proc file. A model-proc file
      describes the model input and output layer format. The model-proc
      file in this tutorial describes the output layer name and labels
      (person and vehicle) of objects it detects. See :doc:`model-proc <../dev_guide/model_proc_file>`
      for more information.
   -  device - device to run inferencing on

-  :doc:`gvatrack <../elements/gvatrack>`
   - Identifies objects in frames where detection is skipped and assigns
   unique ID to objects. This allows us to run object detection on fewer
   frames and increases overall throughput while still tracking the
   position and type of objects in every frame.
-  :doc:`gvawatermark <../elements/gvawatermark>`
   - Overlays detection and classification results on top of video data.
   We will do exactly that. Parse the detected vehicle results metadata
   and create a video frame rendered with the bounding box aligned to
   the vehicle position; parse the classified vehicle result and label
   it on the bounding box.

In addition to ``gvadetect`` and ``gvaclassify``, you can use
``gvainference`` for running inference with any CNN model not supported
by gvadetect or gvaclassify.
``queue`` element must be put directly after ``gvainference`` element in pipeline.
Also, instead of visualizing the inference results, as shown in this tutorial,
you can publish them to MQTT, Kafka or a file using ``gvametaconvert`` and
``gvametapublish`` of Intel® DL Streamer.


Non-Docker tutorial setup
-------------------------

This section prepares the environment to run examples described below. It is suitable if you chose Option #1 (APT repository) in Install Guide Ubuntu.

#. Export ``MODELS_PATH`` to define where to download models. For example:

   .. code:: sh

      export MODELS_PATH=/home/${USER}/intel/models


#. Download the models from `Open Model Zoo <https://github.com/openvinotoolkit/open_model_zoo>`__ to ``MODELS_PATH`` directory:

   .. code:: sh

      python3 -m pip install --upgrade pip
      python3 -m pip install openvino-dev[onnx,tensorflow,pytorch]
      mkdir -p $MODELS_PATH
      omz_downloader --name person-vehicle-bike-detection-2004,vehicle-attributes-recognition-barrier-0039 -o $MODELS_PATH

   .. note::
      Make sure your environment variable ``$PATH`` includes ``$HOME/.local/bin`` - use ``echo $PATH``.

#. Export variables to set paths for *model* and *model_proc* files. It will make pipelines definition easy later in examples:

   .. code:: sh

      export DETECTION_MODEL=${MODELS_PATH}/intel/person-vehicle-bike-detection-2004/FP16/person-vehicle-bike-detection-2004.xml
      export DETECTION_MODEL_PROC=/opt/intel/dlstreamer/samples/gstreamer/model_proc/intel/person-vehicle-bike-detection-2004.json
      export VEHICLE_CLASSIFICATION_MODEL=${MODELS_PATH}/intel/vehicle-attributes-recognition-barrier-0039/FP16/vehicle-attributes-recognition-barrier-0039.xml
      export VEHICLE_CLASSIFICATION_MODEL_PROC=/opt/intel/dlstreamer/samples/gstreamer/model_proc/intel/vehicle-attributes-recognition-barrier-0039.json


   If you want to use your own models, you first need to convert them to
   the IR (Intermediate Representation) format. For detailed
   instructions on how to convert models, `look
   here <https://docs.openvino.ai/2025/openvino-workflow/model-preparation/convert-model-to-ir.html>`__

#. Export the example video file path:

   You may download a sample video from the
   `here <https://github.com/intel-iot-devkit/sample-videos/raw/master/person-bicycle-car-detection.mp4>`__.
   If you provide your own video file as an input, please make sure that
   it is in h264 or mp4 format. You can also download and use freely
   licensed content from the websites such as Pexels\*. Any video with
   cars, or pedestrians can be used with the exercise.

   .. code:: sh

      # This tutorial uses ~/path/to/video as the video path
      # and FILENAME as the placeholder for a video file name.
      # Change this information to fit your setup.
      export VIDEO_EXAMPLE=~/path/to/video/FILENAME

.. _tutorial-setup-for-docker:

Docker tutorial setup
-------------------------
This section prepares the environment to run examples described below. It is suitable if you chose Option #2 (Docker) in Install Guide Ubuntu.

#. Make sure you are not in a Docker container, but on your local host.

#. Export ``MODELS_PATH`` to define where to download models. For example:

   .. code:: sh

      export MODELS_PATH=/home/${USER}/intel/models


#. Download the models from `Open Model Zoo <https://github.com/openvinotoolkit/open_model_zoo>`__ to ``MODELS_PATH`` directory:

   .. code:: sh

      python3 -m pip install --upgrade pip
      python3 -m pip install openvino-dev[onnx,tensorflow,pytorch]
      mkdir -p $MODELS_PATH
      omz_downloader --name person-vehicle-bike-detection-2004,vehicle-attributes-recognition-barrier-0039 -o $MODELS_PATH

   .. note::
      Make sure your environment variable ``$PATH`` includes ``$HOME/.local/bin`` - use ``echo $PATH``.

#. Run Intel® DL Streamer container.

   Run Docker container with the models directory mounted into the container using ``-v`` or ``--volume`` parameter in ``docker run`` command.
   Make sure your mounting parameter is specified as ``-v <path_on_host>:<path_in_the_container>``:

   ..  tabs::

      ..  tab:: Ubuntu 22

         .. code-block:: sh

             docker run -it --rm -v ${MODELS_PATH}:/home/dlstreamer/models --env MODELS_PATH=/home/dlstreamer/models intel/dlstreamer:2025.1.2-ubuntu22

      ..  tab:: Ubuntu 24

         .. code-block:: sh

             docker run -it --rm -v ${MODELS_PATH}:/home/dlstreamer/models --env MODELS_PATH=/home/dlstreamer/models intel/dlstreamer:latest

   Running Intel® DL Streamer in the Docker container with an inference on GPU or NPU devices requires the access as a non-root user to these devices in the container.
   Intel® DL Streamer Pipeline Framework Docker images do not contain a ``render`` group for ``dlstreamer`` non-root user because the ``render`` group does not have a strict group ID, unlike the ``video`` group.
   To run container as non-root with access to a GPU and/or NPU device, you have to specify the ``render`` group ID from your host. The full running command example:

   .. code:: sh

      docker run -it --rm -v ${MODELS_PATH}:/home/dlstreamer/models \
      --device /dev/dri \
      --group-add $(stat -c "%g" /dev/dri/render*) \
      --device /dev/accel \
      --group-add $(stat -c "%g" /dev/accel/accel*) \
      --env ZE_ENABLE_ALT_DRIVERS=libze_intel_vpu.so \
      --env MODELS_PATH=/home/dlstreamer/models \
      intel/dlstreamer:latest

   where newly added parameters are:

   #. ``--device /dev/dri`` - access to GPU device, required when you want to use GPU as inference device (``device=GPU``) or use VA-API graphics hardware acceleration capabilities like ``vapostproc``, ``vah264dec``, ``vah264enc``, ``vah265dec``, ``vah265enc`` etc.

   #. ``--group-add $(stat -c "%g" /dev/dri/render*)`` - non-root access to GPU devices, required in the same scenarios as ``--device /dev/dri`` above

   #. ``--device /dev/accel`` - access to NPU device, required when you want to use NPU as inference device (``device=NPU``)

   #. ``--group-add $(stat -c "%g" /dev/accel/accel*)`` - non-root access to NPU devices, required in the same scenarios as ``--device /dev/accel`` above

   #. ``--env ZE_ENABLE_ALT_DRIVERS=libze_intel_vpu.so`` - exporting environmental variable needed to run inference successfully on NPU devices

#. In the container, export variables to set paths for *model* and *model_proc* files. It will make pipelines definition easy later in examples:

   .. code:: sh

      export DETECTION_MODEL=/home/dlstreamer/models/intel/person-vehicle-bike-detection-2004/FP16/person-vehicle-bike-detection-2004.xml
      export DETECTION_MODEL_PROC=/opt/intel/dlstreamer/samples/gstreamer/model_proc/intel/person-vehicle-bike-detection-2004.json
      export VEHICLE_CLASSIFICATION_MODEL=/home/dlstreamer/models/intel/vehicle-attributes-recognition-barrier-0039/FP16/vehicle-attributes-recognition-barrier-0039.xml
      export VEHICLE_CLASSIFICATION_MODEL_PROC=/opt/intel/dlstreamer/samples/gstreamer/model_proc/intel/vehicle-attributes-recognition-barrier-0039.json


   If you want to use your own models, you need to first convert them in
   the IR (Intermediate Representation) format. For detailed
   instructions to convert models, `look
   here <https://docs.openvino.ai/2025/openvino-workflow/model-preparation/convert-model-to-ir.html>`__

#. In the container, export the example video file path:

   You may download a sample video from the
   `here <https://github.com/intel-iot-devkit/sample-videos/raw/master/person-bicycle-car-detection.mp4>`__.
   If you provide your own video file as an input, please make sure that
   it is in h264 or mp4 format. You can also download and use freely
   licensed content from the websites such as Pexels\*. Any video with
   cars, or pedestrians can be used with the exercise.

   .. code:: sh

      # This tutorial uses ~/path/to/video as the video path
      # and FILENAME as the placeholder for a video file name.
      # Change this information to fit your setup.
      export VIDEO_EXAMPLE=~/path/to/video/FILENAME


.. _object_detection:

Exercise 1 - Build object detection pipeline
---------------------------------------------

This exercise helps you create a GStreamer pipeline that will perform
object detection using ``gvadetect`` element and Intermediate
Representation (IR) formatted object detection model. It provides two
optional add-ons to show you how to use video from a web camera stream
and an RTSP URI.

This exercise introduces you to using the following Pipeline Framework elements:

* gvadetect
* gvawatermark

Pipeline
~~~~~~~~

We will create a pipeline to detect people and vehicles in a video. The
pipeline will accept a video file input, decode it and run vehicle
detection. It will overlay the bounding boxes for detected vehicles on
the video frame and render the video to local device.

Run the below pipeline at the command prompt and review the output:

.. code:: sh

   gst-launch-1.0 \
   filesrc location=${VIDEO_EXAMPLE} ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU ! queue ! \
   gvawatermark ! videoconvert ! autovideosink sync=false

**Expected output**: You will see your video overlaid by bounding boxes
around persons, vehicles, and bikes.

You’re done building and running this pipeline. To expand on this
exercise, use one or both add-ons to this exercise to select different
video sources. If the add-ons don’t suit you, jump ahead to start
`Exercise 2 <#object-classification>`__.

Pipeline with a Web Camera Video Stream Input (First optional add-on to Exercise 1)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

GStreamer supports connected video devices, like web cameras, which
means you use a web camera to perform real-time inference.

In order to use web camera as an input, we will replace the ``filesrc``
element in the object detection pipeline with
`\`v4l2src\` <https://gstreamer.freedesktop.org/documentation/video4linux2/v4l2src.html?gi-language=c>`__
element, that is used for capturing video from webcams. Before running
the below updated pipeline, check the web camera path and update it in
the pipeline. The web camera stream is usually in the ``/dev/``
directory.

Object detection pipeline using web camera:

.. code:: sh

   # Change <path-to-device> below to your web camera device path
   gst-launch-1.0 \
   v4l2src device=<path-to-device> ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU ! queue ! \
   gvawatermark ! videoconvert ! autovideosink sync=false

Pipeline with an RTSP Input (Second optional add-on to Exercise 1)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to use RTSP source as an input, we will replace the ``filesrc``
element in the object detection pipeline with
`\`urisourcebin\` <https://gstreamer.freedesktop.org/documentation/playback/urisourcebin.html?gi-language=c>`__
to access URIs. Before running the below updated pipeline, replace ‘<RTSP_uri>’
with your RTSP URI and verify it before running the command.

Object detection pipeline using sample RTSP URI from Pexels:

.. code:: sh

   gst-launch-1.0 \
   urisourcebin uri=https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4 ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU ! queue ! \
   gvawatermark ! videoconvert ! autovideosink sync=false

.. _object-classification:

Exercise 2: Build object classification pipeline
-------------------------------------------------

This exercise helps you create a GStreamer pipeline that will perform
object classification on the Regions of Interest (ROIs) detected by ``gvadetect`` using
``gvaclassify`` element and Intermediate Representation (IR) formatted
object classification model.

This exercise uses the following Pipeline Framework elements:

* gvadetect
* gvaclassify
* gvawatermark

Pipeline
~~~~~~~~

We will create a pipeline to detect people and vehicles in a video and
classify the detected people and vehicle to provide additional
attributes.

Run the below pipeline at the command prompt and review the output:

.. code:: sh

   gst-launch-1.0 \
   filesrc location=${VIDEO_EXAMPLE} ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU ! queue ! \
   gvaclassify model=${VEHICLE_CLASSIFICATION_MODEL} model-proc=${VEHICLE_CLASSIFICATION_MODEL_PROC} device=CPU object-class=vehicle ! queue ! \
   gvawatermark ! videoconvert ! autovideosink sync=false

**Expected output**: Persons, vehicles, and bikes are bound by colored
boxes, and detection results as well as classification attributes such
as vehicle type and color are displayed as video overlays.

In the above pipeline:

1. ``gvadetect`` detects the ROIs in the video and outputs ROIs with the
   appropriate attributes (person, vehicle, bike) according to its
   model-proc.
2. ``gvadetect`` ROIs are used as inputs for the ``gvaclassify`` model.
3. ``gvaclassify`` classifies the ROIs and outputs additional attributes
   according to model-proc:

   -  ``object-class`` tells ``gvalcassify`` which ROIs to classify.
   -  ``object-class=vehicle`` classifies ROIs with 'vehicle' attribute only.

4. ``gvawatermark`` displays the ROIs and their attributes.

See `model-proc <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc>`__
for the model-procs and its input and output specifications.

.. _object-tracking:

Exercise 3: Use object tracking to improve performance
-------------------------------------------------------

This exercise helps you create a GStreamer pipeline that will use object
tracking for reducing the frequency of object detection and
classification, thereby increasing the throughput, using ``gvatrack``.

This exercise uses the following Pipeline Framework elements:

* ``gvadetect``
* ``gvaclassify``
* ``gvatrack``
* ``gvawatermark``

Pipeline
~~~~~~~~

We will use the same pipeline as in exercise 2, for detecting and
classifying vehicle and people. We will add ``gvatrack`` element after
``gvadetect`` and before ``gvaclassify`` to track objects. ``gvatrack``
will assign object IDs and provide updated ROIs in between detections.
We will also specify parameters of ``gvadetect`` and ``gvaclassify``
elements to reduce frequency of detection and classification.

Run the below pipeline at the command prompt and review the output:

.. code:: sh

   gst-launch-1.0 \
   filesrc location=${VIDEO_EXAMPLE} ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU inference-interval=10 ! queue ! \
   gvatrack tracking-type=short-term-imageless ! queue ! \
   gvaclassify model=${VEHICLE_CLASSIFICATION_MODEL} model-proc=${VEHICLE_CLASSIFICATION_MODEL_PROC} device=CPU object-class=vehicle reclassify-interval=10 ! queue ! \
   gvawatermark ! videoconvert ! autovideosink sync=false

**Expected output**: Persons, vehicles, and bikes are bound by colored
boxes, and detection results as well as classification attributes such
as vehicle type and color are displayed as video overlays, same as
exercise 2. However, notice the increase in the FPS of the pipeline.

In the above pipeline:

1. ``gvadetect`` detects the ROIs in the video and outputs ROIs with the
   appropriate attributes (person, vehicle, bike) according to its
   model-proc on every 10th frame, due to ``inference-interval=10``.
2. ``gvatrack`` tracks each object detected by ``gvadetect``.
3. ``gvadetect`` ROIs are used as inputs for the ``gvaclassify`` model.
4. ``gvaclassify`` classifies the ROIs and outputs additional attributes
   according to model-proc, but skips classification for already
   classified objects for 10 frames, using tracking information from
   ``gvatrack`` to determine whether to classify an object:

   - ``object-class`` tells ``gvaclassify`` which ROIs to classify.
   - ``object-class=vehicle`` classifies ROIs that have the ‘vehicle’ attribute.
   - ``reclassify-interval`` determines how often to reclassify tracked objects. Only valid when used in conjunction with gvatrack.

5. ``gvawatermark`` displays the ROIs and their attributes.

You’re done building and running this pipeline. The next exercise shows
you how to publish your results to a ``.json``.


.. _result-publishing:

Exercise 4: Publish Inference Results
--------------------------------------

This exercise extends the pipeline to publish your detection and
classification results to a ``.json`` file from a GStreamer pipeline.

This exercise uses the following Pipeline Framework elements:

* gvadetect
* gvaclassify
* gvametaconvert
* gvametapublish

Setup
~~~~~

One additional setup step is required for this exercise, to export the
output file path:

.. code:: sh

   # Adjust the command below according to your needs
   export OUTFILE=~/pipeline_output.json

.. _pipeline-3:

Pipeline
~~~~~~~~

We will use the same pipeline as in exercise 2 for detecting and
classifying vehicle and people. However, instead of overlaying the
results and rendering them to a screen, we will send them to a file in
JSON format.

Run the below pipeline at the command prompt and review the output:

.. code:: sh

   gst-launch-1.0 \
   filesrc location=${VIDEO_EXAMPLE} ! decodebin3 ! \
   gvadetect model=${DETECTION_MODEL} model_proc=${DETECTION_MODEL_PROC} device=CPU ! queue ! \
   gvaclassify model=${VEHICLE_CLASSIFICATION_MODEL} model-proc=${VEHICLE_CLASSIFICATION_MODEL_PROC} device=CPU object-class=vehicle ! queue ! \
   gvametaconvert format=json ! \
   gvametapublish method=file file-path=${OUTFILE} ! \
   fakesink

**Expected output**: After the pipeline completes, a JSON file of the
inference results is available. Review the JSON file.

In the above pipeline:

- ``gvametaconvert`` uses the optional parameter ``format=json`` to convert inferenced data to ``GstGVAJSONMeta``.
- ``gvametapublish`` uses the optional parameter ``method=file`` to publish inference results to a file.
- ``filepath=${OUTFILE}`` is a JSON file to which the inference results are published.

For publishing the results to MQTT or Kafka, please refer to the
`metapublish samples <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/metapublish>`__.

You have completed this tutorial. Now, start creating your video
analytics pipeline with Intel® DL Streamer Pipeline Framework!

Next Steps
----------

* `Samples overview <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/README.md>`__
* :doc:`../elements/elements`
* :doc:`../dev_guide/how_to_create_model_proc_file`

-----

.. include:: ../include/disclaimer_footer.rst
