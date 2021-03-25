# gvapython Sample

This sample demonstrates [gvapython](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/gvapython) element and ability to customize pipeline with application provided Python script for pre- or post-processing of inference operations. It typically used for interpretation of inference results and various application logic, especially if required in the middle of GStreamer pipeline.

## How It Works
 In this sample the `gvapython` element inserted into pipeline twice.

 First time it inserted after `gvainference` element running on object detection model, this demonstrates custom conversion of model output into list of bounding boxes. See file `ssd_object_detection.py` with conversion function coded in Python.

 Second time it inserted after `gvaclassify` element running on object classification model, this demonstrates custom conversion model output into object attributes (age and gender in this example). See file `age_gender_classification.py` with conversion function coded in Python.

## Models

The sample uses by default the following pre-trained models from OpenVINO™ Toolkit [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __face-detection-adas-0001__ is primary detection network for finding faces
*   __age-gender-recognition-retail-0013__ age and gender estimation on detected faces

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

## Running

If Python requirements are not installed yet:

```sh
python3 -m pip install --upgrade pip
python3 -m pip install -r ../../../../requirements.txt
cd -
```
Run sample:

```sh
./face_detection_and_classification.sh [INPUT_VIDEO] [DEVICE] [SINK_ELEMENT]
```
The sample takes three command-line *optional* parameters:
1. [INPUT_VIDEO] to specify input video file. 
The input could be
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)  
If parameter is not specified, the sample by default streams video example from HTTPS link (utilizing `urisourcebin` element) so requires internet conection.
2. [DEVICE] to specify device for detection and classification.  
        Please refer to OpenVINO™ toolkit documentation for supported devices.  
        https://docs.openvinotoolkit.org/latest/openvino_docs_IE_DG_supported_plugins_Supported_Devices.html  
        You can find what devices are supported on your system by running following OpenVINO™ toolkit sample:  
        https://docs.openvinotoolkit.org/latest/openvino_inference_engine_ie_bridges_python_sample_hello_query_device_README.html
3. [SINK_ELEMENT] to choose between render mode and fps throughput mode:
    * display - render (default)
    * fps - FPS only

## Sample Output

The sample
* prints gst-launch-1.0 full command line into console
* starts the command and either visualizes video with bouding boxes around detected faces, facial landmarks points and text with classification results (age/gender, emotion) for each detected face or 
prints out fps if you set SINK_ELEMENT = fps

## See also
* [DL Streamer samples](../../../README.md)
