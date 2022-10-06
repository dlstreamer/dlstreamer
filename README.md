# Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework

## Overview
<div align="center"><img src="intro.gif" width=900/></div>

[Intel® Deep Learning Streamer](https://dlstreamer.github.io) (**Intel® DL Streamer**) Pipeline Framework is an open-source streaming media analytics framework, based on [GStreamer*](https://gstreamer.freedesktop.org) multimedia framework, for creating complex media analytics pipelines for the Cloud or at the Edge.

**Media analytics** is the analysis of audio & video streams to detect, classify, track, identify and count objects, events and people. The analyzed results can be used to take actions, coordinate events, identify patterns and gain insights across multiple domains: retail store and events facilities analytics, warehouse and parking management, industrial inspection, safety and regulatory compliance, security monitoring, and many other.

## Backend libraries
Intel® DL Streamer Pipeline Framework is optimized for performance and functional interoperability between GStreamer* plugins built on various backend libraries
* Inference plugins use [OpenVINO™ inference engine](https://docs.openvino.ai) optimized for Intel CPU, GPU and VPU platforms
* Video decode and encode plugins utilize [GPU-acceleration based on VA-API](https://github.com/GStreamer/gstreamer-vaapi)
* Image processing plugins based on [OpenCV](https://opencv.org/) and [DPC++](https://www.intel.com/content/www/us/en/develop/documentation/oneapi-programming-guide/top/oneapi-programming-model/data-parallel-c-dpc.html)
* Hundreds other [GStreamer* plugins](https://gstreamer.freedesktop.org/documentation/plugins_doc.html) built on various open-source libraries for media input and output, muxing and demuxing, decode and encode

[This page](https://dlstreamer.github.io/elements/elements.html) contains a list of elements provided in this repository.

## Installation
Please refer to [Install Guide](https://dlstreamer.github.io/get_started/install/install_guide_ubuntu.html) for installation options
1. Install APT packages
2. Run Docker image
3. Compile from source code
4. Build Docker image from source code

## Samples
[Samples](https://github.com/dlstreamer/dlstreamer/tree/master/samples) available for C/C++ and Python programming, and as gst-launch command lines and scripts. 

## NN models
Intel® DL Streamer supports NN models in OpenVINO™ IR and ONNX* formats:
* Refer to [OpenVINO™ Model Optimizer](<https://docs.openvino.ai/latest/openvino_docs_MO_DG_Deep_Learning_Model_Optimizer_DevGuide.html>) how to convert model into OpenVINO™ IR format 
* Refer to training frameworks documentation how to export model into ONNX* format

Or you can start from over 70 pre-trained models in [OpenVINO™ Open Model Zoo](https://docs.openvino.ai/latest/omz_models_group_intel.html) and corresponding model-proc files (pre- and post-processing specification) in [/opt/intel/dlstreamer/samples/model_proc](https://github.com/dlstreamer/dlstreamer/tree/master/samples/model_proc) folder.
These models include object detection, object classification, human pose detection, sound classification, semantic segmentation, and other use cases on SSD, MobileNet, YOLO, Tiny YOLO, EfficientDet, ResNet, FasterRCNN and other backbones.

## Reporting Bugs and Feature Requests
Report bugs and requests [on the issues page](https://github.com/dlstreamer/dlstreamer/issues)

## Other Useful Links
* [Get Started](https://dlstreamer.github.io/get_started/get_started_index.html)
* [Developer Guide](https://dlstreamer.github.io/dev_guide/dev_guide_index.html)
* [API Reference](https://dlstreamer.github.io/api_ref/api_reference.html)
* Webinars:
    * [Introduction to Intel® DL Streamer Pipeline Framework](https://www.intel.com/content/www/us/en/developer/videos/ready-steady-stream-openvino-toolkit-dl-streamer.html#gs.hwyufz)
    * [Audio event detection synchronized with video based object detection using Intel® DL Streamer Pipeline Framework](https://techdecoded.intel.io/essentials/ai-beyond-computer-vision-with-the-intel-distribution-of-openvino-toolkit)
* YouTube Videos:
    * [Introduction to video analytics pipeline using Intel® DL Streamer Pipeline Framework](https://www.youtube.com/watch?v=fWhPV_IqDy0)
    * [Samples provided in Intel® DL Streamer Pipeline Framework](https://www.youtube.com/watch?v=EqHznsUR1sE)
    * [Object tracking in Intel® DL Streamer Pipeline Framework](https://youtu.be/z4Heorhg3tM)
* The reference media analytics applications, provided by [Open Visual Cloud](https://01.org/openvisualcloud), that leverage Intel® DL Streamer Pipeline Framework elements:
    * [Smart City - Traffic and Stadium Management](https://github.com/OpenVisualCloud/Smart-City-Sample)
    * [Intelligent Ad Insertion](https://github.com/OpenVisualCloud/Ad-Insertion-Sample)
* Try Intel® DL Streamer Pipeline Framework with [Intel<sup>&reg;</sup> DevCloud](https://www.intel.com/content/www/us/en/developer/tools/devcloud/edge/overview.html):
    * You can build your pipeline, test and optimize for free. With an Intel<sup>®</sup> DevCloud account, you get 120 days of access to the latest Intel<sup>®</sup> hardware — CPUs, GPUs, VPUs.
    * No software downloads. No configuration steps. No installations. Check out [Tutorials on Intel<sup>&reg;</sup> DevCloud](https://www.intel.com/content/www/us/en/developer/tools/devcloud/edge/learn/tutorials.html?s=Newest).
* [Intel<sup>®</sup> Edge Software Hub](https://www.intel.com/content/www/us/en/edge-computing/edge-software-hub.html) packages that include Intel® DL Streamer:

    * [Edge Insights for Vision](https://software.intel.com/content/www/us/en/develop/topics/iot/edge-solutions/vision-recipes.html)
    * [Edge Insights for Industrial](https://software.intel.com/content/www/us/en/develop/topics/iot/edge-solutions/industrial-recipes.html)

---
\* Other names and brands may be claimed as the property of others.
