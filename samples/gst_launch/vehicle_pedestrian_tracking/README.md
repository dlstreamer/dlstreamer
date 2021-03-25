# Vehicle and Pedestrian Tracking Sample (gst-launch command line)

This sample demonstrates [gvatrack](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvatrack) element and object tracking capabilities on example of person and vehicle tracking. Object tracking increases performance by running inference on object detection and classification models less frequently (not every frame).

## How It Works
The sample utilizes GStreamer command-line tool `gst-launch-1.0` which can build and run GStreamer pipeline described in a string format.
The string contains a list of GStreamer elements separated by exclamation mark `!`, each element may have properties specified in the format `property`=`value`.

The `gvadetect` element sets `inference-interval` property to 10 frames in this sample, so inference on object detection model executed every 10th frame.

The `gvatrack` element inserted into pipeline after `gvadetect` to track all objects on remaining 9 frames until object detection executed again.

The `gvaclassify` element sets `reclassify-interval` property to 10, so inference on object classification model executed every 10th frames. `gvaclassify` uses unique object ID assigned by `gvatrack` to each object for copying classification results on remaining 9 frames from last frame inference was executed.

Overall this sample builds GStreamer pipeline of the following elements
* `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
* `decodebin` for video decoding
* `videoconvert` for converting video frame into different color formats
* [gvadetect](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvadetect) for person and vehicle detection based on OpenVINO™ Toolkit Inference Engine
* [gvatrack](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvatrack) for tracking objects
* [gvaclassify](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaclassify) inserted into pipeline twice for person and vehicle classification
* [gvawatermark](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvawatermark) for bounding boxes and labels visualization
* `fpsdisplaysink` for rendering output video into screen
> **NOTE**: `sync=false` property in `fpsdisplaysink` element disables real-time synchronization so pipeline runs as fast as possible

## Models

The sample uses by default the following pre-trained models from OpenVINO™ Toolkit [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __person-vehicle-bike-detection-crossroad-0078__ is primary detection network for detecting persons, vehicles and bikes
*   __person-attributes-recognition-crossroad-0230__ classifies person attributes
*   __vehicle-attributes-recognition-barrier-0039__ classifies vehicle attributes

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

The sample contains `model_proc` subfolder with .json files for each model with description of model input/output formats and post-processing rules for classification models.

## Running

```sh
./vehicle_pedestrian_tracking.sh [INPUT_VIDEO] [DETECTION_INTERVAL] [DEVICE] [SINK_ELEMENT]
```

The sample takes four command-line parameters:
1. [INPUT_VIDEO] to specify input video.
The input could be
    * video file path
    * web camera device (ex. /dev/video0)
    * URL of RTSP camera (URL starts with `rtsp://`) or other streaming source (ex `http://`)  
If parameter is not specified, the sample by default streams video example from HTTPS link (utilizing `urisourcebin` element) so requires internet conection.
2. [DETECTION_INTERVAL] to specify interval between inference requests. An interval of N performs inference on every Nth frame. Default value is 10
3. [DEVICE] to specify device for detection and classification.  
    Please refer to OpenVINO™ toolkit documentation for supported devices.  
    https://docs.openvinotoolkit.org/latest/openvino_docs_IE_DG_supported_plugins_Supported_Devices.html  
    You can find what devices are supported on your system by running following OpenVINO™ toolkit sample:  
    https://docs.openvinotoolkit.org/latest/openvino_inference_engine_ie_bridges_python_sample_hello_query_device_README.html
4. [SINK_ELEMENT] to choose between render mode and fps throughput mode:
    * display - render (default)
    * fps - FPS only

## Sample Output

The sample
* prints gst-launch command line into console
* starts the command and visualizes video with bouding boxes around detected objects and text with classification results (color, type and others) for each detected object

## See also
* [DL Streamer samples](../../README.md)
