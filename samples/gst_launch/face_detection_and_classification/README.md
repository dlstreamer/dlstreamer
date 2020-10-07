# Face Detection And Classification Sample (gst-launch command line)

This sample demonstrates face detection and classification pipeline constructed via `gst-launch-1.0` command-line utility.

## How It Works
The sample utilizes GStreamer command-line tool `gst-launch-1.0` which can build and run GStreamer pipeline described in a string format.
The string contains a list of GStreamer elements separated by exclamation mark `!`, each element may have properties specified in the format `property`=`value`.

This sample builds GStreamer pipeline of the following elements
* `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
* `decodebin` for video decoding
* `videoconvert` for converting video frame into different color formats
* [gvadetect](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvadetect) for face detection based on OpenVINO Inference Engine
* [gvaclassify](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvaclassify) inserted into pipeline three times for face classification on three DL models (age-gender, emotion, landmark points)
* [gvawatermark](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvawatermark) for bounding boxes and labels visualization
* `fpsdisplaysink` for rendering output video into screen
> **NOTE**: `sync=false` property in `fpsdisplaysink` element disables real-time synchronization so pipeline runs as fast as possible

## Models

The sample uses by default the following pre-trained models from OpenVINO™ [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __face-detection-adas-0001__ is primary detection network for finding faces
*   __age-gender-recognition-retail-0013__ age and gender estimation on detected faces
*   __emotions-recognition-retail-0003__ emotion estimation on detected faces
*   __landmarks-regression-retail-0009__ generates facial landmark points

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

The sample contains `model_proc` subfolder with .json files for each model with description of model input/output formats and post-processing rules for classification models.

## Running

```sh
./face_detection_and_classification.sh [INPUT_VIDEO]
```

If command-line parameter not specified, the sample by default streams video example from HTTPS link (utilizing `urisourcebin` element) so requires internet conection.
The command-line parameter INPUT_VIDEO allows to change input video and supports
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)

## Sample Output

The sample
* prints gst-launch-1.0 full command line into console
* starts the command and visualizes video with bouding boxes around detected faces, facial landmarks points and text with classification results (age/gender, emotion) for each detected face

## See also
* [DL Streamer samples](../../README.md)
