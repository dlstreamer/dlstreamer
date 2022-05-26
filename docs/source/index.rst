Intel® Deep Learning Streamer
=============================
**Intel® Deep Learning Streamer (Intel® DL Streamer)** is an open-source streaming media analytics framework, based on GStreamer* multimedia framework, for creating complex media analytics pipelines for the Cloud or at the Edge, and it includes:

*	`Intel® Deep Learning Streamer Pipeline Framework <https://github.com/dlstreamer/dlstreamer>`__ for designing, creating, building, and running media analytics pipelines. It includes C++ and Python APIs.
*	`Intel® DL Streamer Pipeline Server <https://github.com/dlstreamer/pipeline-server>`__ for deploying and scaling media analytics pipelines as micro-services on one or many compute nodes. It includes REST APIs for pipelines management.
*	`Intel® DL Streamer Pipeline Zoo <https://github.com/dlstreamer/pipeline-zoo>`__ for benchmarking and optimizing performance of media analytics pipelines. It includes catalog of ready to use media analytics pipelines.

**Media analytics** is the analysis of audio & video streams to detect, classify, track, identify and count objects, events and people. The analyzed results can be used to take actions, coordinate events, identify patterns and gain insights across multiple domains: retail store and events facilities analytics, warehouse and parking management, industrial inspection, safety and regulatory compliance, security monitoring, and many others.

.. image:: _images/overview_pipeline_example.png

**Media analytics pipelines** transform media streams into insights through audio / video processing, inference, and analytics operations across multiple IP blocks.

High-performance Media Analytics solutions are difficult to build, deploy, and benchmark:

*	Require deep expertise in multiple domains, including media processing and Deep Learning based AI inferencing frameworks and tools (e.g., FFmpeg*, GStreamer*, OpenCV*, OpenVINO™ toolkit, PyTorch*, TensorFlow*, ONNX*)
*	Require system level, SW, and HW expertise (e.g., fixed function HW, programmable media, host/accelerator buffer sharing, CPU Core & thread allocation, wide range of system configurations and their trade-offs)
*	Require expertise across CPU classes, SKUs, and their generations, in addition to expertise in integrated & discrete HW accelerators and their interactions with CPUs and rest of the host system (e.g., CPUs, integrated GPUs, discrete GPUs and VPUs)

**Intel® DL Streamer** makes **Media Analytics** easy:

*	Write less code and get better performance
*	Quickly develop, optimize, benchmark, and deploy video & audio analytics pipelines in the Cloud and at the Edge
* Analyze video & audio streams, create actionable results, capture and send results to the cloud
*	Leverage the efficiency and computational power of Intel hardware platforms
*	Write more portable code across Intel hardware platforms, CPU & xPU classes and generations
*	Leverage GStreamer* open-source project community investments in new features & bug-fixes
*	Leverage NNStreamer* selected AI inferencing elements via pipeline inter-operability
*	Customize and extend your solution by reviewing, analyzing, and modifying Intel® DL Streamer open-sourced code

.. figure:: _images/overview_sw_stack.png

   Intel® DL Streamer SW Stack

**Intel® DL Streamer** uses OpenVINO™ Runtime inference back-end optimized for Intel hardware platforms and supports over :doc:`70 NN Intel and open-source community pre-trained models and models </supported_models>` converted from other training frameworks formats using `OpenVINO™ toolkit Model Optimizer <https://docs.openvino.ai/latest/openvino_docs_MO_DG_Deep_Learning_Model_Optimizer_DevGuide.html>`__. These models include object detection, object classification, human pose detection, sound classification, semantic segmentation, and other use cases: SSD, MobileNet, YOLO, Tiny YOLO, EfficientDet, ResNet, FasterRCNN, and other models.

**Intel® DL Streamer** provides over a two dozen of samples, demos and reference apps in for the most common media analytics use cases are included in `Intel® Deep Learning Streamer Pipeline Framework (Intel® DL Streamer Pipeline Framework) <https://github.com/dlstreamer/dlstreamer>`__, `Intel® DL Streamer Pipeline Server <https://github.com/dlstreamer/pipeline-server>`__, `Intel® DL Streamer Pipeline Zoo <https://github.com/dlstreamer/pipeline-zoo>`__, `Open Visual Cloud <https://01.org/openvisualcloud>`__, and `Intel® Edge Software Hub <https://www.intel.com/content/www/us/en/edge-computing/edge-software-hub.html>`__ with C++ and/or Python: Action Recognition, Face Detection and Recognition, Draw Face Attributes, Audio Event Detection, Vehicle and Pedestrian Tracking, Human Pose Estimation, Metadata Publishing, Smart City Traffic and Stadium Management, Intelligent Add insertion, single- & multi- channel video analytics pipelines benchmark, and other use cases.

**Intel® DL Streamer** offers a long list of models and samples optimized for Intel hardware platforms which could be used as a reference or a starting point for a wide range of applications and system configurations. These models and samples are a quick & easy way to reach high performance, then benchmark and optimize your application on your system.

**Intel® DL Streamer** is already used by many partners and customers leading solutions, including `Open Visual Cloud <https://01.org/openvisualcloud>`__ Media Analytics services, `Microsoft Azure Video Analyzer <https://azure.microsoft.com/en-us/products/video-analyzer>`__, `NTT Software Innovation Center <https://www.rd.ntt/e/sic/overview/>`__, `AIVID TECHVISION <https://www.aividtechvision.com/>`__, `Videonetics Technology Pvt. Limited <https://www.videonetics.com/>`__, and others.

Testimonials
============
.. list-table::
   :widths: 20, 80

   * - .. image:: _images/NTT_Logo.png
     - “Intel® DL Streamer (OpenVINO™) is an easy-to-use and extensible application framework, which provides a well-organized set of classes and methods. In particular, DL Streamer allows us to add user-defined post processing with gvapython elements. This feature will help us develop AI-based video analytics applications for NTT's businesses, addressing various customer demands responsively.”

       — Takeharu Eda, Senior Research Engineer, NTT Software Innovation Center

-----

.. include:: include/disclaimer_footer.rst


.. toctree::
   :maxdepth: 2
   :hidden:

   get_started/get_started_index
   dev_guide/dev_guide_index
   elements/elements
   supported_models
   api_ref/api_reference

