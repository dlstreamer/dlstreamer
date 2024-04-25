# Deployment of models trained with Intel® Geti™ Platform (gst-launch command line)

This set of samples demonstrates how to deploy models trained with [Intel® Geti™ Platform](https://geti.intel.com/).

## How It Works
The Intel® Geti™ Platform defines a set of media analytics pipelines corresponding to common usage scenarios: classification, detection, segmentation, etc. 
In all cases, the platform outputs AI models in Intel® OpenVINO™ format: 'openvino.xml' and 'openvino.bin'.

This sample assumes a user has already trained models using Intel® Geti™ Platform and stored the output models in the followign folder structure: 

```sh
-geti
 |-detection/FP16               # model trained with Geti™ 'Detection bounding box' project
   |-openvino.xml                   # detection model metadata
   |-openvino.bin                   # detection model weights
 |-classification_single/FP16   # model trained with Geti™ 'Classification single label' project
   |-openvino.xml                   # classifcation model metadata
   |-openvino.bin                   # classifcation model weights 
 |-classification_multi/FP16    # model trained with Geti™ 'Classification multi label' project       
   |-openvino.xml                   # detection model metadata
   |-openvino.bin                   # detection model weights 
```

The set of samples demonstrates how to deploy above models to run inference with GStreamer command line tool `gst-launch-1.0` and Intel® DL StreamerDL Streamer framework components.

| Geti™ project               | Intel® DL Streamer component                                                                        | Outcome                                            |
| --------------------------- | -------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| Detection bounding box      | gvadetect model=./geti/detection/FP16/openvino.xml                                           | detects objects in a frame and mark bounding-boxes |
| Classification single label | gvaclassify model=./geti/classification_single/FP16/openvino.xml inference-region=full-frame | classifies each frame by a single label            |
| Classification multi label  | gvaclassify model=./geti/classification_multi/FP16/openvino.xml inference-region=full-frame  | classifies each frame by multiple labels           |  

## Samples

The 'geti_sample.sh' script sample builds GStreamer pipeline composed of the following elements:
* `filesrc` or `urisourcebin` or `v4l2src` for input from file/URL/web-camera
* `decodebin` for video decoding
* `videoconvertscale` for converting video frame into different color formats
* [gvadetect](https://dlstreamer.github.io/elements/gvadetect.html) uses for full-frame object detection and marking objects with labels
* [gvaclassify](https://dlstreamer.github.io/elements/gvaclassify.html) uses for full-frame object classficiation
* [gvawatermark](https://dlstreamer.github.io/elements/gvawatermark.html) for points and theirs connections visualization
* `autovideosink` for rendering output video into screen
* `vaapih264enc` and `filesink` for encoding video stream and storing in a local file
> **NOTE**: `sync=false` property in `autovideosink` element disables real-time synchronization so pipeline runs as fast as possible

Example deployment of Geti™ bounding-box detection model using GPU device, saving results into a file on local disk. 
```sh
./geti_sample.sh detection GPU
```

Example deployment of Geti™ single-label classification model using NPU device, saving results into a file on local disk. 
```sh
./yolox_sample.sh classification_single NPU
```

Example deployment of Geti™ multi-label classification model using CPU device, saving results into a file on local disk. 
```sh
./yolox_sample.sh classification_multi CPU
```

## See also
* [Samples overview](../../README.md)
