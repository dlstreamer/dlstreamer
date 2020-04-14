# DL Streamer Samples

Samples are simple applications that demonstrate how to use the DL Streamer. The samples are available in the `<INSTALL_DIR>/data_processing/dl_streamer/samples` directory.

Samples separated into several categories
1. gst_launch command-line samples construct GStreamer pipeline via `gst-launch-1.0` command-line utility
    * [Face Detection And Classification Sample](./gst_launch/face_detection_and_classification/README.md) - constructs object detection and classification pipeline example to detect faces and estimate age, gender, emotions and landmark points
    * [Vehicle and Pedestrian Tracking Sample](./gst_launch/vehicle_pedestrian_tracking/README.md) - demonstrates object tracking via `gvatrack` element
    * [gvapython Samples](./gst_launch/gvapython/face_detection_and_classification/README.md) demostrates pipeline customization with `gvapython` element and application provided python script for inference pre- or post-processing
2. C++ samples construct GStreamer pipeline using "C" API
    * [Draw Face Attributes C++ Sample](./cpp/draw_face_attributes/README.md) - constructs pipeline and sets "C" callback to access frame metadata and visualize inference results
3. Python samples construct GStreamer pipeline using Python API
    * [Draw Face Attributes Python Sample](./python/draw_face_attributes/README.md) - constructs pipeline and sets Python callback to access frame metadata and visualize inference results
4. Benchmark
    * [Benchmark Sample](./benchmark/README.md) - measures overall performance of single-channel or multi-channel video analytics pipelines

## How To Build And Run

Samples with C/C++ code provide `build_and_run.sh` shell script to build application via cmake before execution.

Other samples (without C/C++ code) provide .sh script for constucting and executing gst-launch or Python command line.

## DL Models

DL Streamer samples use pre-trained models from OpenVINO™ [Open Model Zoo](https://github.com/opencv/open_model_zoo)

Before running samples, run script `download_models.sh` once to download all models required for samples. The script located in `samples` top folder.

## Input video

First command-line parameter in DL Streamer samples specifies input video and supports
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)

If command-line parameter not specified, most samples by default stream video example from predefined HTTPS link, so require internet conection.

> **NOTE**: Most samples set property `sync=false` in video sink element to disable real-time synchronization and run pipeline as fast as possible. Change to `sync=true` to run pipeline with real-time speed.
