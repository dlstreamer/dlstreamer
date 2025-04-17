Samples 2.0
===========

Architecture 2.0 brings new samples separated into 3 categories by programming models
(*Direct Programming, GStreamer low-level, GStreamer high-level*):

.. list-table::
   :header-rows: 1

   * - Category
     - Folder
     - Sample(s)
     - #1 used
     - #2 used
     - #3 used
   * - Direct Programming
     - samples/ffmpeg_openvino/
     - `decode_inference <https://github.com/dlstreamer/dlstreamer/tree/master/samples/ffmpeg_openvino/cpp/decode_inference>`_
     - No
     - No
     - No
   * - Direct Programming
     - samples/ffmpeg_openvino/
     - `decode_resize_inference <https://github.com/dlstreamer/dlstreamer/tree/master/samples/ffmpeg_openvino/cpp/decode_resize_inference>`_
     - Yes
     - Yes
     - No
   * - Direct Programming
     - samples/ffmpeg_dpcpp/
     - `rgb_to_grayscale <https://github.com/dlstreamer/dlstreamer/tree/master/samples/ffmpeg_dpcpp/rgb_to_grayscale>`_
     - Yes
     - Yes
     - No
   * - GStreamer low-level
     - samples/gstreamer/gst_launch/face_detection_and_classification_bins/
     - `face_detection_and_classification_cpu.sh <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/face_detection_and_classification_bins/face_detection_and_classification_cpu.sh>`_
       `face_detection_and_classification_gpu.sh <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/face_detection_and_classification_bins/face_detection_and_classification_gpu.sh>`_
     - No
     - Yes
     - No
   * - GStreamer low-level
     - samples/gstreamer/gst_launch/action_recognition/
     - `action_recognition.sh <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/action_recognition/action_recognition.sh>`_
     - No
     - Yes
     - Yes
   * - GStreamer low-level
     - samples/gstreamer/gst_launch/instance_segmentation/
     - `instance_segmentation.sh <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/instance_segmentation/instance_segmentation.sh>`_
     - No
     - Yes
     - Yes
   * - GStreamer high-level
     - samples/gstreamer/gst_launch
     - All other `GStreamer samples <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer>`_
       if set environment variable ``export DLSTREAMER_GEN=2``
     - No
     - No
     - Yes
   * - GStreamer gva* (architecture 1.0)
     - samples/gstreamer/gst_launch
     - All `GStreamer samples <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer>`_
       using PV-quality gva* elements
     - No
     - No
     - No
