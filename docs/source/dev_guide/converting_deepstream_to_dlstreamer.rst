Converting NVIDIA DeepStream Pipelines to Intel® Deep Learning Streamer (Intel® DL Streamer)
=====================================================

This document will describe the steps to convert a pipeline from NVIDIA
DeepStream to Intel DL Streamer.
We also have a running example through the document that will be updated at
each step to help show the modifications being described.

.. note::
   The intermediate steps of the pipeline are not meant to run. They are simply
   there as a reference example of the changes being made in each section.

Preparing Your Model
--------------------

To use Intel DL Streamer and OpenVINO™ Toolkit the
model needs to be in Intermediate Representation (IR) format. To convert
your model to this format, use the `Model Optimizer <https://docs.openvino.ai/latest/openvino_docs_MO_DG_Deep_Learning_Model_Optimizer_DevGuide.html>`__
tool from OpenVINO™ Toolkit.

Intel DL Streamer's inferencing plugins optionally can do some pre- and
post-processing operations before/after running inferencing. These
operations are specified in a model-proc file. Visit :doc:`this page <model_preparation>`
for more information on creating a model-proc file and examples with
various models from `Open Model Zoo <https://github.com/openvinotoolkit/open_model_zoo>`__.

GStreamer Pipeline Adjustments
------------------------------

In the following sections we will be converting the below pipeline that
is using DeepStream elements to Intel DL Streamer. It is taken from one of the
examples
`here <https://github.com/NVIDIA-AI-IOT/redaction_with_deepstream>`__.
It takes an input stream from file, decodes, runs inferencing, overlays
the inferences on the video, re-encodes and outputs a new .mp4 file.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin ! \
   nvstreammux batch-size=1 width=1920 height=1080 ! queue ! \
   nvinfer config-file-path=./config.txt ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

Mux and Demux Elements
~~~~~~~~~~~~~~~~~~~~~~

-  Remove ``nvstreammux`` and ``nvstreamdemux`` and all their
   properties.

   -  These elements are used in the case of multiple input streams to
      connect all inputs to the same inferencing element. In DL
      Streamer, the inferencing elements share properties and instances
      if they share the same ``model-instance-id`` property.
   -  In our example, we only have one source so we will skip this for
      now. See more on how to do this with Intel DL Streamer in the section
      :ref:`Multiple Input Streams <Multiple-Input-Streams>` below.

At this stage we have removed ``nvstreammux`` and the ``queue`` that
followed it. Notably, the ``batch-size`` property is also removed. It
will be added in the next section as a property of the Intel DL Streamer
inference elements.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin ! \
   nvinfer config-file-path=./config.txt ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

Inferencing Elements
~~~~~~~~~~~~~~~~~~~~

-  Remove ``nvinfer`` and replace it with ``gvainference``,
   ``gvadetect`` or ``gvaclassify`` depending on the following use
   cases:

   -  For doing detection on full frames and outputting a region of
      interest, use
      :doc:`gvadetect <../elements/gvadetect>`.
      This replaces ``nvinfer`` when it is used in primary mode.

      -  Replace ``config-file-path`` property with ``model`` and
         ``model-proc``.
      -  ``gvadetect`` generates GstVideoRegionOfInterestMeta.

   -  For doing classification on previously detected objects, use
      :doc:`gvaclassify <../elements/gvaclassify>`.
      This replaces nvinfer when it is used in secondary mode.

      -  Replace ``config-file-path`` property with ``model`` and
         ``model-proc``.
      -  ``gvaclassify`` requires GstVideoRegionOfInterestMeta as input.

   -  For doing generic full frame inferencing, use
      :doc:`gvainference <../elements/gvainference>`.
      This replaces ``nvinfer`` when used in primary mode.

      -  ``gvainference`` generates GstGVATensorMeta.

In this example we will use gvadetect to infer on the full frame and
output region of interests. ``batch-size`` was also added for
consistency with what was removed above (the default value is 1 so it is
not needed). We replaced ``config-file-path`` property with ``model``
and ``model-proc`` properties. See the section above about “Preparing
your model” for converting the model to IR format and creating a
model-proc file.

.. note:: 
   The ``model-proc`` file is not always needed depending on the model's inputs and outputs.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

Video Processing Elements
~~~~~~~~~~~~~~~~~~~~~~~~~

-  Replace video processing elements with vaapi equivalents for GPU or
   native GStreamer elements for CPU.

   -  ``nvvideoconvert`` with ``vaapipostproc`` or ``mfxvpp`` (GPU) or
      ``videoconvert`` (CPU).

      -  If the ``nvvideoconvert`` is being used to convert to/from
         ``memory:NVMM`` it can just be removed.

   -  ``nvv4ldecoder`` can be replaced with ``vaapi{CODEC}dec``, for
      example ``vaapih264dec`` for decode only or ``vaapidecodebin`` for
      decode and vaapipostproc. Alternatively, the native GStreamer
      element ``decodebin`` can be used to automatically choose an
      available decoder.

-  Some caps filters that follow an inferencing element may need to be
   adjusted or removed. Intel DL Streamer inferencing elements do not support
   color space conversion in post-processing. You will need to have a
   ``vaapipostproc`` or ``videoconvert`` element to handle this.

Here we removed a few caps filters and instances of ``nvvideoconvert``
used for conversions from DeepStream’s NVMM because Intel DL Streamer uses
standard GStreamer structures and memory types. We will leave the
standard gstreamer element ``videoconvert`` to do color space conversion
on CPU, however if available, we suggest using ``vaapipostproc`` to run
on Intel Graphics. Also, we will use the GStreamer standard element
``decodebin`` to choose an appropriate demuxer and decoder depending on
the input stream as well as what is available on the system.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! \
   nvdsosd ! queue ! \
   videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

Metadata Elements
~~~~~~~~~~~~~~~~~

-  Replace ``nvtracker`` with
   :doc:`gvatrack <../elements/gvatrack>`

   -  Remove ``ll-lib-file`` property. Optionally replace with
      ``tracking-type`` if you want to specify the algorithm used. By
      default it will use the ‘short-term’ tracker.
   -  Remove all other properties.

-  Replace ``nvdsosd`` with
   :doc:`gvawatermark <../elements/gvawatermark>`

   -  Remove all properties

-  Replace ``nvmsgconv`` with
   :doc:`gvametaconvert <../elements/gvametaconvert>`

   -  ``gvametaconvert`` can be used to convert metadata from
      inferencing elements to JSON and to output metadata to the
      GST_DEBUG log.
   -  It has optional properties to configure what information goes into
      the JSON object including frame data for frames with no detections
      found, tensor data, the source the inferences came from, and tags,
      a user defined JSON object that is attached to each output for
      additional custom data.

-  Replace ``nvmsgbroker`` with
   :doc:`gvametapublish <../elements/gvametapublish>`

   -  ``gvametapublish`` can be used to output the JSON messages
      generated by ``gvametaconvert`` to stdout, file, MQTT or Kafka.

The only metadata processing that is done in this pipeline is to overlay
the inferences on the video for which we use ``gvawatermark``.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! \
   gvawatermark ! queue ! \
   videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

.. _Multiple-Input-Streams:

Multiple Input Streams
----------------------

| Unlike DeepStream, where all sources need to be linked to the sink
  pads of the ``nvstreammux`` element, Intel DL Streamer shares all model and
  Inference Engine properties between elements that have the same
  ``model-instance-id`` property. Meaning that you do not need to mux
  all sources together before inferencing and you can remove any
  instances of ``nvstreammux`` and ``nvstreamdemux``. Below is a pseudo
  example of a pipeline with two streams.
| For DeepStream, the pipeline would look like this:

.. code:: shell

   nvstreammux ! nvinfer config-file-path=./config.txt ! nvstreamdemux filesrc ! decode ! mux.sink_0 filesrc ! decode ! mux.sink_1 demux.src_0 ! encode ! filesink demux.src_1 ! encode ! filesink

When using Intel DL Streamer, the pipeline will look like this:

.. code:: shell

   filesrc ! decode ! gvadetect model=./model.xml model-proc=./model_proc.json model-instance-id=model1 ! encode ! filesink filesrc ! decode ! gvadetect model-instance-id=model1 ! encode ! filesink
