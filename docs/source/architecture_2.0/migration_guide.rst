Migration to 2.0
================

.. note::
   Architecture 2.0 currently available in preview mode as transition not completed yet. There is no strict backward
   compatibility in preview mode (some changes/renaming may occur). It could be used for evaluation and prototyping,
   it's not recommended for production usage yet.

Multiple ways to program media analytics pipeline
-------------------------------------------------

Starting release 2022.2, Intel® DL Streamer offers multiple ways to program media analytics pipeline.
The following table summarizes major differences between programming models

.. list-table::
   :stub-columns: 1

   * - Programming model:
     - ``GStreamer high-level (gva elements)``
     - ``GStreamer low-level (low-level elements and processbin)``
     - ``Direct programming``
   * - Why choice:
     -
          * Very easy to program - small or no C++/Python coding to build GStreamer pipeline
          * Out-of-box support for many typical use cases
     -
          * Pretty easy to program
          * Allows to create custom C++ elements and mix with existent C++ elements in GStreamer pipeline
     -
          * Full flexibility of C++/Python programming to any library/framework APIs:
            FFmpeg, GStreamer, OpenVINO™, Level-Zero, OpenCL, OpenCV Mat, SYCL, VA-API
          * Application is responsible for pipeline management and data flow (queues, async execution, multi-stream, etc)
          * Application can use memory interop library and any C++ elements provided by Intel® DL Streamer
   * - Sample(s):
     -
          * GStreamer command-line: `samples/gstreamer/gst_launch <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch>`_
          * GStreamer C++: `samples/gstreamer/cpp <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/cpp>`_
          * GStreamer Python: `samples/gstreamer/python <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/python>`_
     -
          * For example, what if we should implement object classification (on ROI cropped images) with background removal?
          * Adding custom C++ element `opencv_remove_background <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/src/opencv/opencv_remove_background/opencv_remove_background.cpp>`_
            in ~100 C++ lines `allows to achieve that <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/instance_segmentation>`_
          * We reused many existent low-level elements for pre-processing, inference, post-processing on segmentation and classification models.
     -
          * FFmpeg+OpenVINO™: `samples/ffmpeg_openvino <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/ffmpeg_openvino>`_
          * FFmpeg+DPCPP/SYCL: `samples/ffmpeg_dpcpp <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/ffmpeg_dpcpp>`_


See :doc:`Samples 2.0 <samples_2.0>` for table with all samples.

Backward compatibility between 1.0 and 2.0
------------------------------------------

Intel® DL Streamer sets the goal of backward compatible transition from GStreamer gva* 1.0 elements to architecture 2.0
bin-elements, including all elements and all properties except deprecated properties. After transition is completed, all
existent application and pipeline are expected to continue working without any changes or with minor changes replacing
deprecated properties with new properties.

Many GStreamer high-level elements are now implemented as bin elements with names different from gva* element. The table below
lists mapping from old to new names

.. list-table::
   :header-rows: 1

   * - gva* element 1.0
     - bin element 2.0
   * - gvainference
     - video_inference
   * - gvadetect
     - object_detect
   * - gvaclassify
     - object_classify
   * - gvatrack
     - object_track
   * - gvawatermark
     - meta_overlay
   * - gvametaaggregate
     - meta_aggregate (WIP)
   * - ...
     - ...

We introduced environment variable to automatically redirect gva* elements ``gvainference``, ``gvadetect``, ``gvaclassify`` to
corresponding bin elements ``video_inference``, ``object_detect``, ``object_classify``. Any existent application automatically
switches to architecture 2.0 elements if the following environment variable is set:

.. code-block:: none

  export DLSTREAMER_GEN=2

One of 2023.x releases is expected to make such redirection from gva* element to bin elements the default behavior, essentially
automatically migrating all existent applications to architecture 2.0. After such transition the same environment variable will
allow to switch back to gva* elements and architecture 1.0 (for significant time period) via setting: 

.. code-block:: none

  export DLSTREAMER_GEN=1

Deprecated properties not available in 2.0
------------------------------------------

The following table lists deprecated properties (not supported in 2.0) and new property/value to set this parameter
(supported in 2.0, and 1.0 as well for some properties).

.. list-table::
   :header-rows: 1

   * - Element(s)
     - Deprecated property (supported in 1.0 only)
     - New property / element(s)
     - New property supported by both 1.0 and 2.0?
   * - gvainference, gvadetect, gvaclassify
     - labels=LABELS_FILE.txt
     - labels-file=LABELS_FILE.txt
     - Yes
   * - gvainference, gvadetect, gvaclassify
     - cpu-throughput-streams=N
     - ie-config=CPU_THROUGHPUT_STREAMS=N
     - Yes
   * - gvainference, gvadetect, gvaclassify
     - gpu-throughput-streams=N
     - ie-config=GPU_THROUGHPUT_STREAMS=N
     - Yes
   * - gvainference, gvadetect, gvaclassify
     - pre-process-config=VAAPI_FAST_SCALE_LOAD_FACTOR=1
     - scale-method=fast
     - Yes
   * - gvatrack
     - tracking-type=
     - object_track generate-objects= adjust-objects= spatial-feature=
     - Yes
   * - gvatrack
     - device=GPU
     - object_track device=GPU
     - No
   * - gvawatermark
     - device=GPU
     - meta_overlay device=GPU
     - No
