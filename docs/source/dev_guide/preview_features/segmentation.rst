Segmentation
============

.. attention:: 
  This is a preview feature available on the
  `preview/segmentation <https://github.com/dlstreamer/dlstreamer/tree/preview/segmentation>`__
  branch for evaluation purpose only.
  It is not production quality yet and the API may change in future releases.

``gvasegment`` element performs image segmentation and text detection.

It supports the following four types of post-processing: 

* semantic_default - processes a one-channel feature map, where each pixel is a label of one of the classes. 
* semantic_args_plane_max - processes a N-channels feature map, where each channel is a probability of one of the classes.
* instance_default - processes bounding box plus segmentation mask output such as the output of
  `instance-segmentation-security-0010 <https://github.com/openvinotoolkit/open_model_zoo/blob/2021.2/models/intel/instance-segmentation-security-0010/description/instance-segmentation-security-0010.md>`__ model.
* pixel_link - post processer for text-detection models.

A sample pipeline for gvasegment usage is available
`here <https://github.com/dlstreamer/dlstreamer/tree/preview/segmentation/samples/gst_launch/segmentation>`__.

.. code-block:: none

   Pad Templates:
     SRC template: 'src'
       Availability: Always
       Capabilities:
         video/x-raw
                    format: { (string)BGRx, (string)BGRA, (string)BGR, (string)NV12, (string)I420 }
                     width: [ 1, 2147483647 ]
                    height: [ 1, 2147483647 ]
                 framerate: [ 0/1, 2147483647/1 ]

     SINK template: 'sink'
       Availability: Always
       Capabilities:
         video/x-raw
                    format: { (string)BGRx, (string)BGRA, (string)BGR, (string)NV12, (string)I420 }
                     width: [ 1, 2147483647 ]
                    height: [ 1, 2147483647 ]
                 framerate: [ 0/1, 2147483647/1 ]

   Element has no clocking capabilities.
   Element has no URI handling capabilities.

   Pads:
     SINK: 'sink'
       Pad Template: 'sink'
     SRC: 'src'
       Pad Template: 'src'

   Element Properties:
     batch-size          : Number of frames batched together for a single inference. Not all models support batching. Use model optimizer to ensure that the model has batching support.
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 1024 Default: 1
     cpu-throughput-streams: Sets the cpu-throughput-streams configuration key for OpenVINO™ Toolkit's cpu device plugin. Configuration allows for multiple inference streams for better performance. Default mode is auto. See OpenVINO™ Toolkit CPU plugin documentation for more details
                           flags: readable, writable, deprecated
                           Unsigned Integer. Range: 0 - 4294967295 Default: 0
     device              : Target device for inference. Please see OpenVINO™ Toolkit documentation for list of supported devices.
                           flags: readable, writable
                           String. Default: "CPU"
     device-extensions   : Comma separated list of KEY=VALUE pairs specifying the Inference Engine extension for a device
                           flags: readable, writable
                           String. Default: ""
     gpu-throughput-streams: Sets the gpu-throughput-streams configuration key for OpenVINO™ Toolkit's gpu device plugin. Configuration allows for multiple inference streams for better performance. Default mode is auto. See OpenVINO™ Toolkit GPU plugin documentation for more details
                           flags: readable, writable, deprecated
                           Unsigned Integer. Range: 0 - 4294967295 Default: 0
     ie-config           : Comma separated list of KEY=VALUE parameters for Inference Engine configuration
                           flags: readable, writable
                           String. Default: ""
     inference-interval  : Interval between inference requests. An interval of 1 (Default) performs inference on every frame. An interval of 2 performs inference on every other frame. An interval of N performs inference on every Nth frame.
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 4294967295 Default: 1
     inference-region    : Identifier responsible for the region on which inference will be performed
                           flags: readable, writable
                           Enum "InferenceRegionType" Default: 0, "full-frame"
                              (0): full-frame       - Perform inference for full frame
                              (1): roi-list         - Perform inference for roi list
     model               : Path to inference model network file
                           flags: readable, writable
                           String. Default: null
     model-instance-id   : Identifier for sharing a loaded model instance between elements of the same type. Elements with the same model-instance-id will share all model and inference engine related properties
                           flags: readable, writable
                           String. Default: null
     model-proc          : Path to JSON file with description of input/output layers pre-processing/post-processing
                           flags: readable, writable
                           String. Default: null
     name                : The name of the object
                           flags: readable, writable
                           String. Default: "gvasegment0"
     nireq               : Number of inference requests
                           flags: readable, writable
                           Unsigned Integer. Range: 0 - 1024 Default: 0
     no-block            : (Experimental) Option to help maintain frames per second of incoming stream. Skips inference on an incoming frame if all inference requests are currently processing outstanding frames
                           flags: readable, writable
                           Boolean. Default: false
     parent              : The parent of the object
                           flags: readable, writable
                           Object of type "GstObject"
     pre-process-backend : Select a pre-processing method (color conversion, resize and crop), one of 'ie', 'opencv', 'vaapi', 'vaapi-surface-sharing'. If not set, it will be selected automatically: 'vaapi' for VASurface and DMABuf, 'ie' for SYSTEM memory.
                           flags: readable, writable
                           String. Default: ""
     qos                 : Handle Quality-of-Service events
                           flags: readable, writable
                           Boolean. Default: false
     reshape             : Enable network reshaping.  Use only 'reshape=true' without reshape-width and reshape-height properties if you want to reshape network to the original size of input frames. Note: this feature has a set of limitations. Before use, make sure that your network supports reshaping
                           flags: readable, writable
                           Boolean. Default: false
     reshape-height      : Height to which the network will be reshaped.
                           flags: readable, writable
                           Unsigned Integer. Range: 0 - 4294967295 Default: 0
     reshape-width       : Width to which the network will be reshaped.
                           flags: readable, writable
                           Unsigned Integer. Range: 0 - 4294967295 Default: 0
