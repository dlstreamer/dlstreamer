# Audio Detection Sample (gst-launch command line)

This sample demonstrates how to construct an audio event detection pipeline using the command-line utility`gst-launch-1.0`.

## How It Works
`gst-launch-1.0` is a command-line utility included with the popular GStreamer media framework. It makes the construction and execution of media pipelines easy based on a simple and intuitive string format. Pipelines are represented as strings containing the names of GStreamer elements separated by exclamation marks `!`. Users can specify properties of an element using `property`=`value` pairs after an element name and before the next exclamation mark.

This sample builds a GStreamer pipeline using the following elements
* `filesrc` or `urisourcebin`
* `decodebin` for audio decoding
* `audioresample`, `audioconvert` and `audiomixer` for converting and resizing audio input
* [gvaaudiodetect](https://github.com/opencv/gst-video-analytics/wiki/gvaaudiodetect) for audio event detection using ACLNet
* [gvametaconvert](https://github.com/opencv/gst-video-analytics/wiki/gvametaconvert) for converting ACLNet detection results into JSON for further processing and display
* [gvametapublish](https://github.com/opencv/gst-video-analytics/wiki/gvametapublish) for printing detection results to stdout
* `fakesink` for terminating the pipeline

## Model

This sample uses the ACLNet model trained for audio event detection and made available through the Open Model Zoo. For more details see [here](https://download.01.org/opencv/models_contrib/sound_classification/aclnet/pytorch/15062020/aclnet_des_53_fp32.onnx).
*   __aclnet_des_53_fp32.onnx__ is end-to-end convolutional neural network architecture for audio classification

> **NOTE**: Before running this sample you'll need to download and prepare the model. Execute `download_audio_models.sh` once to download and prepare models for all audio samples.

## Model Proc

Along with the model network and weights, gvaudiodetect uses an additional `model-proc` json file that describes how to prepare the input for the model and interpret its output.

`model-proc` is a JSON file that describes the output layer name and label mapping (Cat, Dog, Baby Crying) for the output of the model. For each possible output of the model (specified using a zero based index) you can set a label and output specific threshold [Check Here](model_proc/aclnet_des_53_fp32.json).

## Running

```sh
./audio_event_detection.sh [INPUT_PATH]
```
Where [INPUT_PATH] can be:
* local audio file ('./example.audio.wav')
* network source (ex URL starting with `http://`)

By default, if no [INPUT_PATH] is specified, the sample uses a local file `how_are_you_doing.wav` (utilizing `filesrc` element).

## Sample Output

The sample
* prints full gst-launch-1.0 command to the console
* starts the command and outputs audio detection results to the console