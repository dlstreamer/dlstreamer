# Segmentation Sample (gst-launch command line)

This sample demonstrates segmentation pipeline constructed via `gst-launch-1.0` command-line utility.

## How It Works
The sample utilizes GStreamer command-line tool `gst-launch-1.0` which can build and run GStreamer pipeline described in a string format.
The string contains a list of GStreamer elements separated by exclamation mark `!`, each element may have properties specified in the format `property`=`value`.

This sample builds GStreamer pipeline of the following elements
* `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
* `decodebin` for video decoding
* `videoconvert` for converting video frame into different color formats
* `gvasegment` for segmentation based on OpenVINO™ Toolkit Inference Engine
* [gvawatermark](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvawatermark) for bounding boxes and labels visualization
* `fpsdisplaysink` for rendering output video into screen
> **NOTE**: `sync=false` property in `fpsdisplaysink` element disables real-time synchronization so pipeline runs as fast as possible

## Models

The sample uses by default the following pre-trained models from OpenVINO™ Toolkit [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __icnet-camvid-ava-0001__ segmentation model
*   __instance-segmentation-security-0083__ instance segmentation model
*   __text-detection-0003__ text segmentation model

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

The sample contains `model_proc` subfolder with .json files for each model with description of model input/output formats and post-processing rules for classification models.

## Сonfiguration file parameters

* Default converter's field:
    * `converter` - type of an applied converter
    * `show_zero_class` - true/false, whether objects that are segmented as a null class should be shown
* Special field:
    * `semantic_default`
    * `semantic_args_plane_max`
    * `instance_default` converter
        * `net_height`, `net_width` - (input_preproc), height and width of the neural network's output tensor
        * `conf_threshold` - confidence threshold for detected objects
        * `labels` -  class of objects
    * `pixel_link` converter
        * `cls_threshold` - confidence threshold for the 'text' class
        * `link_threshold` - confidence threshold for pixel linkage, [mode details](https://arxiv.org/abs/1801.01315)

## Running

```sh
./semantic_segmentation.sh [INPUT_VIDEO]
./instantce_segmentation.sh [INPUT_VIDEO]
./text_segmentation.sh [INPUT_VIDEO]
```

If command-line parameter not specified, the sample by default streams video example from HTTPS link (utilizing `urisourcebin` element) so requires internet conection.
The command-line parameter INPUT_VIDEO allows to change input video and supports
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)

## Sample Output

The sample
* prints gst-launch-1.0 full command line into console
* starts the command and visualizes video with segmented objects

## See also
* [DL Streamer samples](../../README.md)
