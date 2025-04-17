Converting NVIDIA DeepStream Pipelines to Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework
================================================================================================================

This document describes the steps to convert a pipeline from NVIDIA
DeepStream to Intel® DL Streamer Pipeline Framework.
We also have a running example through the document that will be updated at
each step to help show the modifications being described.

.. note::
   The intermediate steps of the pipeline are not meant to run. They are simply
   there as a reference example of the changes being made in each section.

Contents
--------

-  :ref:`Preparing Your Model <preparing_model>`
-  :ref:`Configuring Model for Intel® DL Streamer <configure_model>`
-  :ref:`GStreamer Pipeline Adjustments <gstreamer_pipeline_adj>`
-  :ref:`Mux and Demux Elements <mux_demux_elements>`
-  :ref:`Inferencing Elements <inferencing_elements>`
-  :ref:`Video Processing Elements <video_processing_elements>`
-  :ref:`Metadata Elements <metadata_elements>`
-  :ref:`Multiple Input Streams <multiple-input-streams>`
-  :ref:`DeepStream to DLStreamer Elements Mapping Cheetsheet <mapping_cheetsheet>`

 
.. _preparing_model:
Preparing Your Model
--------------------

To use Intel® DL Streamer Pipeline Framework and OpenVINO™ Toolkit the model needs to be in
Intermediate Representation (IR) format. To convert your model to this format, please follow
:doc:`model preparation <model_preparation>` steps.

.. _configure_model:
Configuring Model for Intel® DL Streamer
------------------------------------------------------------

NVIDIA DeepStream uses a combination of model configuration files and DeepStream element properties
to specify interference actions as well as pre- and post-processing steps before/after running inference
as documented here: `here <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvinfer.html>`__.

Similarly, Intel® DL Streamer Pipeline Framework uses GStreamer element properties for inference
settings and :doc:`model proc <model_proc_file>` files for pre- and post-processing steps. 

The following table shows how to map commonly used NVIDIA DeepStream configuration properties
to Intel® DL Streamer settings.

.. list-table::
   :header-rows: 1
   
   * - NVIDIA DeepStream config file
     - NVIDIA DeepStream element property
     - Intel® DL Streamer model proc file
     - Intel® DL Streamer element property
     - Description
   * - model-engine-file <path>
     - model-engine-file <path>
     - 
     - model <path>
     - Path to inference model network file.
   * - labelfile-path <path>
     - 
     - 
     - labels-file <path>
     - Path to .txt file containing object classes.
   * - network-type <0..3>
     - 
     - 
     - | gvadetect for detection, instance segmentation
       | gvaclassify for classification, semantic segmentation
     - Type of inference operation.
   * - batch-size <N>
     - batch-size <N>
     - 
     - batch-size <N>
     - Number of frames batched together for a single inference.
   * - maintain-aspect-ratio
     - 
     - resize: aspect-ratio
     - 
     - Number of frames batched together for a single inference.
   * - num-detected-classes
     - 
     - 
     - 
     - Number of classes detected by the model, inferred from label file by Intel® DL Streamer.
   * - interval <N>
     - interval <N>
     - 
     - inference-interval <N+1>
     - Inference action executed every Nth frame, please note Intel® DL Streamer value is greater by 1.
   * - 
     - threshold
     - 
     - threshold
     - Threshold for detection results.

.. _gstreamer_pipeline_adj:
GStreamer Pipeline Adjustments
------------------------------

In the following sections we will be converting DeepStream pipeline to Pipeline Framework.
The DeepStream pipeline is taken from one of the examples
`here <https://github.com/NVIDIA-AI-IOT/deepstream_reference_apps>`__.
It reads a video stream from the input file, decodes it, runs inference, overlays
the inferences on the video, re-encodes and outputs a new .mp4 file.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin3 ! \
   nvstreammux batch-size=1 width=1920 height=1080 ! queue ! \
   nvinfer config-file-path=./config.txt ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4


The below mapping represents the typical changes that need to be made to the pipeline to convert it to Intel® DL Streamer Pipeline Framework.
The pipeline is broken down into sections based on the elements used in the pipeline.


.. image:: deepstream_mapping_dlstreamer.png


The next chapters give more details on how to replace each element.


.. _mux_demux_elements:
Mux and Demux Elements
~~~~~~~~~~~~~~~~~~~~~~

-  Remove ``nvstreammux`` and ``nvstreamdemux`` and all their
   properties.

   -  These elements combine multiple input streams into a single batched video stream (NVIDIA-specific).
      Intel® DL Streamer takes a different approach: it employs generic GStreamer syntax to define parallel streams.
      The cross-stream batching happens at the inferencing elements by setting the same ``model-instance-id`` property.
   -  In this example, there is only one video stream so we can skip this for now.
      See more on how to construct multi-stream pipelines in the following section
      :ref:`Multiple Input Streams <multiple-input-streams>` below.

At this stage we have removed ``nvstreammux`` and the ``queue`` that
followed it. Notably, the ``batch-size`` property is also removed. It
will be added in the next section as a property of the Pipeline Framework
inference elements.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin3 ! \
   nvinfer config-file-path=./config.txt ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

.. _inferencing_elements:
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

   -  For doing generic full frame inference, use
      :doc:`gvainference <../elements/gvainference>`.
      This replaces ``nvinfer`` when used in primary mode.

      -  ``gvainference`` generates GstGVATensorMeta.

In this example we will use gvadetect to infer on the full frame and
output region of interests. ``batch-size`` was also added for
consistency with what was removed above (the default value is 1 so it is
not needed). We replaced ``config-file-path`` property with ``model``
and ``model-proc`` properties as described in “Configuring Model for Intel® DL Streamer” above.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin3 ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! queue ! \
   nvvideoconvert ! "video/x-raw(memory:NVMM), format=RGBA" ! \
   nvdsosd ! queue ! \
   nvvideoconvert ! "video/x-raw, format=I420" ! videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

.. _video_processing_elements:
Video Processing Elements
~~~~~~~~~~~~~~~~~~~~~~~~~

-  Replace NVIDIA-specific video processing elements with native GStreamer elements.

   -  ``nvvideoconvert`` with ``vapostproc`` (GPU) or ``videoconvert`` (CPU).

      -  If the ``nvvideoconvert`` is being used to convert to/from
         ``memory:NVMM`` it can just be removed.

   -  ``nvv4ldecoder`` can be replaced with ``va{CODEC}dec``, for
      example ``vah264dec`` for decode only. Alternatively, the
      native GStreamer element ``decodebin3`` can be used to automatically
      choose an available decoder.

-  Some caps filters that follow an inferencing element may need to be
   adjusted or removed. Pipeline Framework inferencing elements do not support
   color space conversion in post-processing. You will need to have a
   ``vapostproc`` or ``videoconvert`` element to handle this.

Here we removed a few caps filters and instances of ``nvvideoconvert``
used for conversions from DeepStream’s NVMM because Pipeline Framework uses
standard GStreamer structures and memory types. We will leave the
standard gstreamer element ``videoconvert`` to do color space conversion
on CPU, however if available, we suggest using ``vapostproc`` to run
on Intel Graphics. Also, we will use the GStreamer standard element
``decodebin`` to choose an appropriate demuxer and decoder depending on
the input stream as well as what is available on the system.

.. code:: shell

   filesrc location=input_file.mp4 ! decodebin3 ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! queue ! \
   nvdsosd ! queue ! \
   videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4

.. _metadata_elements:
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

   filesrc location=input_file.mp4 ! decodebin3 ! \
   gvadetect model=./model.xml model-proc=./model_proc.json batch-size=1 ! queue ! \
   gvawatermark ! queue ! \
   videoconvert ! avenc_mpeg4 bitrate=8000000 ! qtmux ! filesink location=output_file.mp4


.. _multiple-input-streams:
Multiple Input Streams
----------------------

| Unlike DeepStream, where all sources need to be linked to the sink pads of the ``nvstreammux`` element, 
  Pipeline Framework uses existing GStreamer mechanisms to define multiple parallel video processing streams.
  This approach allow to reuse native GStreamer elements within the pipeline. 
  The input stream can share same Inference Engine if they have same ``model-instance-id`` property.
  This allows creating inference batching across streams.

| For DeepStream, the simple pipeline involving two streams would look like code snippet below. 
  The first line defines a common inference element for two (muxed and batched) streams. 
  The second line defines per-stream input operations prior to muxing. 
  The third line defines per-stream output operations after de-muxing.

.. code:: shell

   nvstreammux ! nvinfer batch-size=2 config-file-path=./config.txt ! nvstreamdemux \ 
   filesrc ! decode ! mux.sink_0 filesrc ! decode ! mux.sink_1 \
   demux.src_0 ! encode ! filesink demux.src_1 ! encode ! filesink

When using Pipeline Framework, the command line defines operations for two parallel streams using native GStreamer syntax.
By setting ``model-instance-id`` to the same value, both streams share the same instance ``gvadetect`` element.
Hence, the shared inference parameters (model, batch size, ...) can be defined only in the first line.


.. code:: shell

   filesrc ! decode ! gvadetect model-instance-id=model1 model=./model.xml batch-size=2 ! encode ! filesink \ 
   filesrc ! decode ! gvadetect model-instance-id=model1 ! encode ! filesink



.. _mapping_cheetsheet:
DeepStream to DLStreamer Elements Mapping Cheetsheet
----------------------------------------------------

Below table lists quick reference for mapping typical DeepStream elements to Intel® DL Streamer elements or GStreamer.

.. list-table::
  :header-rows: 1
  :widths: auto
  
  * - DeepStream Element
    - DLStreamer Element
  * - `nvinfer <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvinfer.html>`__
    - :doc:`gvadetect <../elements/gvadetect>`, :doc:`gvaclassify <../elements/gvaclassify>`, :doc:`gvainference <../elements/gvainference>`
  * - `nvdsosd <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvdsosd.html>`__
    - :doc:`gvawatermark <../elements/gvawatermark>`
  * - `nvtracker <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvtracker.html>`__
    - :doc:`gvatrack <../elements/gvatrack>`
  * - `nvmsgconv <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvmsgconv.html>`__
    - :doc:`gvametaconvert <../elements/gvametaconvert>`
  * - `nvmsgbroker <https://docs.nvidia.com/metropolis/deepstream/dev-guide/text/DS_plugin_gst-nvmsgbroker.html>`__
    - :doc:`gvametapublish <../elements/gvametapublish>`


.. list-table::
  :header-rows: 1
  :widths: auto
  
  * - DeepStream Element
    - GStreamer Element
  * - nvvideoconvert
    - videoconvert
  * - nvv4l2decoder
    - decodebin3
  * - nvv4l2h264dec
    - vah264dec
  * - nvv4l2h265dec
    - vah265dec
  * - nvv4l2h264enc
    - va264enc
  * - nvv4l2h265enc
    - va265enc
  * - nvv4l2vp8dec
    - vavp8dec
  * - nvv4l2vp9dec
    - vavp9dec
  * - nvv4l2vp8enc
    - vavp8enc
  * - nvv4l2vp9enc
    - vavp9enc
