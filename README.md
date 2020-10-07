# [OpenVINO<sup>&#8482;</sup> Toolkit](https://software.intel.com/en-us/openvino-toolkit) - DL Streamer repository

## Overview
<div align="center"><img src="intro.gif" width=900/></div>

This repository is a home to Deep Learning (DL) Streamer. DL Streamer is a streaming media analytics framework, based on GStreamer* multimedia framework, for creating complex media analytics pipelines. It ensures pipeline interoperability and provides optimized media, and inference operations using Intel® Distribution of OpenVINO Toolkit Inference Engine backend, across Intel® architecture - CPU, iGPU and Intel® Movidius™ VPU. DL Streamer prebuilt binaries can be installed with the Intel® Distribution of OpenVINO™ toolkit installer. 

Here's the canonical video analytics pipeline consturctued using DL Streamer. It performs detection and classification operations on a video stream, using face detection and emotion classification deep learning models. The results of this pipeline are demoed in the above video clip:
```sh
gst-launch-1.0 filesrc location=cut.mp4 ! decodebin ! videoconvert ! gvadetect model=face-detection-adas-0001.xml ! gvaclassify model=emotions-recognition-retail-0003.xml model-proc=emotions-recognition-retail-0003.json ! gvawatermark ! xvimagesink sync=false
```

The complete solution leverages:
* Open source GStreamer* framework for pipeline management
* GStreamer* plugins for input and output such as media files and real-time streaming from camera or network
* Video decode and encode plugins, either CPU optimized plugins or GPU-accelerated plugins [based on VAAPI](https://github.com/GStreamer/gstreamer-vaapi)
* Deep Learning models converted from training frameworks TensorFlow*, Caffe* etc. from Open Model Zoo
And, the following elements in the DL Streamer repository:

In addition, the solution uses the following Deep Learning-specific elements, also available in this repository:
* Inference plugins leveraging [OpenVINO<sup>&#8482;</sup> Toolkit](https://software.intel.com/en-us/openvino-toolkit) for high-performance inference using deep learning models
* Visualization of the inference results, with bounding boxes and labels of detected objects, on top of video stream

Please refer to [Elements](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/Elements) for the complete DL Streamer elements list.


## License
The GStreamer Video Analytics Plugin, part of [OpenVINO<sup>&#8482;</sup> Toolkit](https://software.intel.com/en-us/openvino-toolkit) - DL Streamer, is licensed under the [MIT license](LICENSE).

GStreamer is an open source framework licensed under LGPL. See [license terms](https://gstreamer.freedesktop.org/documentation/frequently-asked-questions/licensing.html?gi-language=c). You are solely responsible for determining if your use of Gstreamer requires any additional licenses.  Intel is not responsible for obtaining any such licenses, nor liable for any licensing fees due, in connection with your use of Gstreamer

## Prerequisites
### Hardware
* Refer to [OpenVINO™ Toolkit Hardware](https://software.intel.com/content/www/us/en/develop/tools/openvino-toolkit/hardware.html) sections - Intel® Processors, Intel® Processor Graphics and Intel® Movidius™ Vision Processing Unit (VPU)
* On platforms with Intel Gen graphics, see the gstreamer-vaapi for [hardware accelerated video decode and encode requirements](https://github.com/GStreamer/gstreamer-vaapi)

### Software
* Intel® Distribution of OpenVINO<sup>&#8482;</sup> Toolkit Release 2021.1 (Inference Engine 2.1.0) or above
* Linux* system with kernel 4.15 or above
* GStreamer* framework 1.14 or above

## Getting Started
* Start here: [Install Guide](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/Install-Guide)
* [DL Streamer Tutorial](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/GStreamer%20Video%20Analytics%20Tutorial)
* YouTube Videos: 
    * [Full pipeline simulation using GStreamer](https://www.youtube.com/watch?v=fWhPV_IqDy0)
    * [Full pipeline simulation using GStreamer (Samples)](https://www.youtube.com/watch?v=EqHznsUR1sE)
* Samples:
    * [command-line](samples/gst_launch), [C++](samples/cpp/draw_face_attributes) and [Python](samples/python/draw_face_attributes/) samples.

For additional documentation, please see [wiki](https://github.com/openvinotoolkit/dlstreamer_gst/wiki) and don't miss the documentation indexed on the right side of the wiki home page.

## Develop in the Cloud
Try DL Streamer with [Intel&reg; DevCloud](https://devcloud.intel.com/edge/get_started/devcloud/). You can build your pipeline, test and optimize for free. With an Intel® DevCloud account, you get 120 days of access to the latest Intel® hardware — CPUs, GPUs, FPGAs. No software downloads. No configuration steps. No installations. Check out [DL Stramer Tutorial on Intel&reg; DevCloud](https://devcloud.intel.com/edge/advanced/advanced_tutorials/). 

## Other Useful Links 
* [Video Analytics Serving](https://github.com/openvinotoolkit/dlstreamer_gst/wiki/Elements): Video Analytics Serving is a python package and microservice for deploying optimized media analytics pipelines. It supports pipelines defined in DL Streamer and provides APIs to discover, start, stop, customize and monitor pipeline execution.
* The reference media analytics applications, provided by [Open Visual Cloud](https://01.org/openvisualcloud), that leverage DL Streamer elements:
    *  [Smart City - Traffic and Stadium Management](https://github.com/OpenVisualCloud/Smart-City-Sample)
    * [Intelligent Ad Insertion](https://github.com/OpenVisualCloud/Ad-Insertion-Sample)
* [Intel® Edge Software Hub](https://www.intel.com/content/www/us/en/edge-computing/edge-software-hub.html) packages that include DL Streamer:

    * [Edge Insights for Vision](https://software.intel.com/content/www/us/en/develop/topics/iot/edge-solutions/vision-recipes.html)
    * [Edge Insights for Industrial](https://software.intel.com/content/www/us/en/develop/topics/iot/edge-solutions/industrial-recipes.html)



## Reporting Bugs and Feature Requests
Report bugs and requests [on the issues page](https://github.com/openvinotoolkit/dlstreamer_gst/issues)


## How to contribute
Pull requests aren't monitored, so if you have bug fix or an idea to improve this project, post a description on the [issues page](https://github.com/openvinotoolkit/dlstreamer_gst/issues).

---
\* Other names and brands may be claimed as the property of others.
