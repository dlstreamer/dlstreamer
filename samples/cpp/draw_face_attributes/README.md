# Draw Face Attributes C++ Sample

This sample demonstrates how construct and control GStreamer pipeline from C++ application, and how to access metadata attached by inference elements to image buffer.

## How It Works
The sample utilizes GStreamer function `gst_parse_launch` to construct the pipeline from string representation. Then callback function is set on source pin of `gvawatermark` element in the pipeline.

The callback is invoked on every frame, it loops through inference metadata attached to the frame, converts raw tensor data into attributes and text labels (conversion depends on the model), and visualizes labels around detected objects.

Note that this sample doesn't contain .json files with post-processing rules as post-processing of classification results performed by sample itself (inside callback function), not by `gvaclassify` element.

## Models

The sample uses by default the following pre-trained models from OpenVINO™ [Open Model Zoo](https://github.com/openvinotoolkit/open_model_zoo)
*   __face-detection-adas-0001__ is primary detection network for finding faces
*   __age-gender-recognition-retail-0013__ age and gender estimation on detected faces
*   __emotions-recognition-retail-0003__ emotion estimation on detected faces
*   __facial-landmarks-35-adas-0002-0009__ generates facial landmark points
*   __head-pose-estimation-adas-0001__ estimates head pose

> **NOTE**: Before running samples (including this one), run script `download_models.sh` once (the script located in `samples` top folder) to download all models required for this and other samples.

## Running

```sh
./build_and_run.sh [INPUT_VIDEO]
```

The script `build_and_run.sh` compiles the C++ sample into subfolder under `$HOME/intel/dl_streamer`, then runs the executable file.

If no input parameters specified, the sample by default streams video example from HTTPS link (utilizing `urisourcebin` element) so requires internet conection.
The command-line parameter INPUT_VIDEO allows to change input video and supports
* local video file
* web camera device (ex. `/dev/video0`)
* RTSP camera (URL starting with `rtsp://`) or other streaming source (ex URL starting with `http://`)

## Sample Output

The sample
* prints GSreamer pipeline string as passed to function `gst_parse_launch`
* starts the pipeline and visualizes video with bouding boxes around detected faces, facial landmarks points, head pose, and text with classification results (age/gender, emotion) for each detected face

## See also
* [DL Streamer samples](../../README.md)
