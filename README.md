# DL Streamer is now part of Open Edge Platform, for latest updates and releases please visit new repo: https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer
    
## Intel® Deep Learning Streamer (Intel® DL Streamer) Pipeline Framework

### Overview
<div align="center"><img src="intro.gif" width=900/></div>

[Intel® Deep Learning Streamer](https://dlstreamer.github.io) (**Intel® DL Streamer**) Pipeline Framework is an open-source streaming media analytics framework, based on [GStreamer*](https://gstreamer.freedesktop.org) multimedia framework, for creating complex media analytics pipelines for the Cloud or at the Edge.

**Media analytics** is the analysis of audio & video streams to detect, classify, track, identify and count objects, events and people. The analyzed results can be used to take actions, coordinate events, identify patterns and gain insights across multiple domains: retail store and events facilities analytics, warehouse and parking management, industrial inspection, safety and regulatory compliance, security monitoring, and many other.

### Backend libraries
Intel® DL Streamer Pipeline Framework is optimized for performance and functional interoperability between GStreamer* plugins built on various backend libraries
* Inference plugins use [OpenVINO™ inference engine](https://docs.openvino.ai) optimized for Intel CPU, GPU and VPU platforms
* Video decode and encode plugins utilize [GPU-acceleration based on VA-API](https://github.com/GStreamer/gstreamer-vaapi)
* Image processing plugins based on [OpenCV](https://opencv.org/) and [DPC++](https://www.intel.com/content/www/us/en/develop/documentation/oneapi-programming-guide/top/oneapi-programming-model/data-parallel-c-dpc.html)
* Hundreds other [GStreamer* plugins](https://gstreamer.freedesktop.org/documentation/plugins_doc.html) built on various open-source libraries for media input and output, muxing and demuxing, decode and encode

[This page](https://dlstreamer.github.io/elements/elements.html) contains a list of elements provided in this repository.

## Prerequisites
Please refer to [System Requirements](https://dlstreamer.github.io/get_started/system_requirements.html) for details.

## Installation
Please refer to [Install Guide](https://dlstreamer.github.io/get_started/install/install_guide_ubuntu.html) for installation options
1. [Install APT packages](https://dlstreamer.github.io/get_started/install/install_guide_ubuntu.html#option-1-install-intel-dl-streamer-pipeline-framework-from-debian-packages-using-apt-repository)
2. [Run Docker image](https://dlstreamer.github.io/get_started/install/install_guide_ubuntu.html#option-2-install-docker-image-from-docker-hub-and-run-it)
3. [Compile from source code](https://dlstreamer.github.io/dev_guide/advanced_install/advanced_install_guide_compilation.html)
4. [Build Docker image from source code](https://dlstreamer.github.io/dev_guide/advanced_install/advanced_build_docker_image.html)

To see the full list of installed components check the [dockerfile content for Ubuntu24](https://raw.githubusercontent.com/open-edge-platform/edge-ai-libraries/refs/heads/main/libraries/dl-streamer/docker/dlstreamer_dev_ubuntu24.Dockerfile)

## Samples
[Samples](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples) available for C/C++ and Python programming, and as gst-launch command lines and scripts. 

### NN models
Intel® DL Streamer supports NN models in OpenVINO™ IR and ONNX* formats:
* Refer to [OpenVINO™ Model Optimizer](<https://docs.openvino.ai/latest/openvino_docs_MO_DG_Deep_Learning_Model_Optimizer_DevGuide.html>) how to convert model into OpenVINO™ IR format 
* Refer to training frameworks documentation how to export model into ONNX* format

Or you can start from over 70 pre-trained models in [OpenVINO™ Open Model Zoo](https://docs.openvino.ai/latest/omz_models_group_intel.html) and corresponding model-proc files (pre- and post-processing specification) in [/opt/intel/dlstreamer/samples/model_proc](https://github.com/dlstreamer/dlstreamer/tree/master/samples/model_proc) folder.
These models include object detection, object classification, human pose detection, sound classification, semantic segmentation, and other use cases on SSD, MobileNet, YOLO, Tiny YOLO, EfficientDet, ResNet, FasterRCNN and other backbones.

## Compilation
You can build DLStreamer using the provided Makefile. Reference `make help` for a list of all available targets.
Currently building is supported for Ubuntu 24.04 and Ubuntu 22.04 systems.

#### Compile DLStreamer and it's dependencies with
```bash
make build # build22 for Ubuntu 22.04 systems
```

#### Install DLStreamer onto your system with
> Note: DLStreamer will be installed in the `/opt/intel/dlstreamer` directory during this process.
```bash
make install
```

#### Build the DLStreamer DEB package with
> Note: the DEB package build process requires that docker be present on the system.
```bash
make deb # deb22 for the Ubuntu 22.04 package
```

#### Build the DLStreamer RPM package with
> Note: the RPM package build process requires that docker be present on the system.
```bash
make rpm
```

#### Build the DLStreamer docker image with
```bash
make image # image22 for the Ubuntu 22.04 image
```

#### Clean the build artifacts with
```bash
make clean
```
#### Configure environment
When building manually, to use DLStreamer you will need to configure your environment so that DLStreamer can find all necessary elements. Assuming that you cloned this repository to `$HOME/edge-ai-libraries`, here's an example set of variables:

```bash
export LIBVA_DRIVER_NAME=iHD
export GST_PLUGIN_PATH="$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/opt/intel/dlstreamer/gstreamer/lib/gstreamer-1.0:/usr/lib/x86_64-linux-gnu/gstreamer-1.0"
export LD_LIBRARY_PATH="/opt/intel/dlstreamer/gstreamer/lib:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib:/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"
export LIBVA_DRIVERS_PATH="/usr/lib/x86_64-linux-gnu/dri"
export GST_VA_ALL_DRIVERS="1"
export PATH="/opt/intel/dlstreamer/gstreamer/bin:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/bin:$HOME/.local/bin:$HOME/python3venv/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$HOME/edge-ai-libraries/libraries/dl-streamer/build/intel64/Release/lib/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/opt/intel/dlstreamer/gstreamer/lib/pkgconfig:$PKG_CONFIG_PATH"
export GST_PLUGIN_FEATURE_RANK=${GST_PLUGIN_FEATURE_RANK},ximagesink:MAX
```

## Other Useful Links
* [Get Started](https://dlstreamer.github.io/get_started/get_started_index.html)
* [Developer Guide](https://dlstreamer.github.io/dev_guide/dev_guide_index.html)
* [API Reference](https://dlstreamer.github.io/api_ref/api_reference.html)
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
