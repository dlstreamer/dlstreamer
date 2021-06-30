# DL Streamer Samples

Samples are simple applications that demonstrate how to use the DL Streamer. The samples are available in the `<INSTALL_DIR>/data_processing/dl_streamer/samples` directory.

Samples separated into several categories
1. gst_launch command-line samples (samples construct GStreamer pipeline via [gst-launch-1.0](https://gstreamer.freedesktop.org/documentation/tools/gst-launch.html) command-line utility)
    * [Action Recognition Sample](./gst_launch/action_recognition/README.md) - demonstrates action recognition via [gvaactionrecognitionbin](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaactionrecognitionbin) element
    * [Face Detection And Classification Sample](./gst_launch/face_detection_and_classification/README.md) - constructs object detection and classification pipeline example with [gvadetect](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvadetect) and [gvaclassify](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaclassify) elements to detect faces and estimate age, gender, emotions and landmark points
    * [Audio Event Detection Sample ](./gst_launch/audio_detect/README.md) - constructs audio event detection pipeline example with [gvaaudiodetect](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaaudiodetect) element and uses  [gvametaconvert](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametaconvert), [gvametapublish](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametapublish) elements to convert audio event metadata with inference results into JSON format and to print on standard out
    * [Vehicle and Pedestrian Tracking Sample](./gst_launch/vehicle_pedestrian_tracking/README.md) - demonstrates object tracking via [gvatrack](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvatrack) element
    * [Human Pose Estimation Sample](./gst_launch/human_pose_estimation/README.md) - demonstrates human pose estimation with full-frame inference via [gvaclassify](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaclassify) element
    * [Metadata Publishing Sample](./gst_launch/metapublish/README.md) - demonstrates how [gvametaconvert](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametaconvert) and [gvametapublish](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametapublish) elements are used for converting metadata with inference results into JSON format and publishing to file or Kafka/MQTT message bus
    * [gvapython Sample](./gst_launch/gvapython/face_detection_and_classification/README.md) - demostrates pipeline customization with [gvapython](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvapython) element and application provided Python script for inference post-processing
2. C++ samples
    * [Draw Face Attributes C++ Sample](./cpp/draw_face_attributes/README.md) - constructs pipeline and sets "C" callback to access frame metadata and visualize inference results
3. Python samples
    * [Draw Face Attributes Python Sample](./python/draw_face_attributes/README.md) - constructs pipeline and sets Python callback to access frame metadata and visualize inference results
4. Benchmark
    * [Benchmark Sample](./benchmark/README.md) - measures overall performance of single-channel or multi-channel video analytics pipelines

## How To Build And Run

Samples with C/C++ code provide `build_and_run.sh` shell script to build application via cmake before execution.

Other samples (without C/C++ code) provide .sh script for constucting and executing gst-launch or Python command line.

## DL Models

DL Streamer samples use pre-trained models from OpenVINO™ Toolkit [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)

Before running samples, run script `download_models.sh` once to download all models required for samples. The script located in `samples` top folder.
> **NOTE**: To install all necessary requirements for `download_models.sh` script run this command:
```sh
pip3 install -r $INTEL_OPENVINO_DIR/deployment_tools/open_model_zoo/tools/downloader/requirements.in
```

## Input video

First command-line parameter in DL Streamer samples specifies input video and supports
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)

If command-line parameter not specified, most samples by default stream video example from predefined HTTPS link, so require internet conection.

> **NOTE**: Most samples set property `sync=false` in video sink element to disable real-time synchronization and run pipeline as fast as possible. Change to `sync=true` to run pipeline with real-time speed.
