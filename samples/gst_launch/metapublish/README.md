# Metadata Publishing Sample (gst-launch command line)

This sample demonstrates how [gvametaconvert](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametaconvert) and [gvametapublish](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametapublish) elements are used in a typical DL Streamer pipeline. By placing these elements to the end of a pipeline that performs face detection and emotion classification, you will quickly see how these elements enable publishing of pipeline metadata to an output file, in-memory fifo, or a popular message bus.

These elements are useful for cases where you need to record outcomes (e.g., emitting inferences) of your DL Streamer pipeline to applications running locally or across distributed systems.

## How It Works
The sample utilizes GStreamer command-line tool `gst-launch-1.0` which can build and run GStreamer pipeline described in a string format.
The string contains a list of GStreamer elements separated by exclamation mark `!`, each element may have properties specified in the format `property`=`value`.

Overall this sample builds GStreamer pipeline of the following elements:
* `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
* `decodebin` for video decoding
* [gvadetect](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvadetect) for detecting faces using the OpenVINO™ Toolkit Inference Engine
* [gvaclassify](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaclassify) for recognizing the age and gender of detected faces using the the OpenVINO™ Toolkit Inference Engine.
* [gvametaconvert](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametaconvert) for conversion of tensor and inference metadata to JSON format.
* [gvametapublish](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametapublish) for publishing the JSON metadata as output to console, file, fifo, MQTT or Kafka.
* `fakesink` to terminate the pipeline output without actually rendering video frames.

> **NOTE**: The sample sets property 'json-indent=4' in [gvametaconvert](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvametaconvert) element for generating JSON in pretty print format with 4 spaces indent. Remove this property to generate JSON without pretty print.

## Models

The sample uses by default the following pre-trained models from OpenVINO™ Toolkit [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __face-detection-adas-0001__ is primary detection network for detecting faces that appear within video frames.
*   __age-gender-recognition-retail-0013__ classifies age and gender of detected face(s).

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

The sample contains `model_proc` subfolder with .json files for each model with description of model input/output formats and post-processing rules for classification models.

## Running

This sample takes up to four command-line parameters. If no parameters specified, the sample displays pretty printed JSON messages to console (METHOD=file, OUTPUT=stdout)

> **NOTE**: Before running this sample with output to MQTT or Kafka, refer to [*this page*](./listener.md) how to set up a MQTT or Kafka listener to consume and review results in the console.

    ```sh
    ./metapublish.sh [INPUT] [METHOD] [OUTPUT] [TOPIC]
    ```

1. [INPUT] is full path to the source video.
The input could be
    * URI to web based streaming source ([Default](https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4))
    * local video file path
    * path to RTSP camera (URL starts with `rtsp://`)
    * web camera device (ex. /dev/video0)
1. [METHOD] to specify which output method to use when publishing inferences.
The method could be
    * file (Default)
    * kafka
    * mqtt
1. [OUTPUT] Indicates the path to the 'file' or to specify the address of message bus broker.
By default this is
    * stdout (Default if METHOD is File)
    * localhost:1883 (Default if METHOD is MQTT)
    * localhost:9092 (Default if METHOD is Kafka)
1. [TOPIC] (used only if METHOD is MQTT or Kafka) to specify the message bus topic to publish:
    * dlstreamer (Default)

### Examples
1. Launch sample with no parameters to see stdout with pretty json.
   ```
   ./metapublish.sh
   ```

1. Override the file or absolute path of output file.
    ```
    ./metapublish.sh https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4 \
    file /tmp/output_inferences.json
    ```

1. Output results to MQTT broker running at localhost:1883 with your listener subscribed to 'dlstreamer' topic.
    ```
    ./metapublish.sh https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4 mqtt
    ```

## Sample Output
The sample
* prints gst-launch command line into console
* starts the command and emits inference events that include the evaluated age and gender for each face detected within video input frames.

## See also
* [DL Streamer samples](../../README.md)
