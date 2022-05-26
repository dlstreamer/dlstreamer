gvainference
============

Runs deep learning inference using any model with an RGB or BGR input.

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
        video/x-raw(memory:DMABuf)
                   format: { (string)RGBA, (string)I420 }
                    width: [ 1, 2147483647 ]
                   height: [ 1, 2147483647 ]
                framerate: [ 0/1, 2147483647/1 ]
        video/x-raw(memory:VASurface)
                   format: { (string)NV12 }
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
        video/x-raw(memory:DMABuf)
                   format: { (string)RGBA, (string)I420 }
                    width: [ 1, 2147483647 ]
                   height: [ 1, 2147483647 ]
                framerate: [ 0/1, 2147483647/1 ]
        video/x-raw(memory:VASurface)
                   format: { (string)NV12 }
                    width: [ 1, 2147483647 ]
                   height: [ 1, 2147483647 ]
                framerate: [ 0/1, 2147483647/1 ]

  Element has no clocking capabilities.
  Element has no URI handling capabilities.

  Pads:
    SRC: 'src'
    SINK: 'sink'

  Element Properties:
    async-handling      : The bin will handle Asynchronous state changes
                          flags: readable, writable
                          Boolean. Default: false
    attach-tensor-data  : If true, metadata will contain both post-processing results and raw tensor data. If false, metadata will contain post-processing results only.
                          flags: readable, writable
                          Boolean. Default: true
    batch-size          : Number of frames batched together for a single inference. If the batch-size is 0, then it will be set by default to be optimal for the device. Not all models support batching. Use model optimizer to ensure that the model has batching support.
                          flags: readable, writable
                          Unsigned Integer. Range: 0 - 1024 Default: 0
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
    inference-interval  : Run inference for every Nth frame
                          flags: readable, writable
                          Unsigned Integer. Range: 1 - 4294967295 Default: 1
    inference-region    : Identifier responsible for the region on which inference will be performed
                          flags: readable, writable
                          Enum "GvaInferenceBinRegion" Default: 0, "full-frame"
                             (0): full-frame       - Perform inference for full frame
                             (1): roi-list         - Perform inference for roi list
    labels              : Path to file containing model's output layer labels or comma separated list of KEY=VALUE pairs where KEY is name of output layer and VALUE is path to labels file. If provided, labels from model-proc won't be loaded
                          flags: readable, writable
                          String. Default: ""
    message-forward     : Forwards all children messages
                          flags: readable, writable
                          Boolean. Default: false
    model               : Path to inference model network file
                          flags: readable, writable
                          String. Default: ""
    model-instance-id   : Identifier for sharing resources between inference elements of the same type. Elements with the instance-id will share model and other properties. If not specified, a unique identifier will be generated.
                          flags: readable, writable
                          String. Default: ""
    model-proc          : Path to JSON file with description of input/output layers pre-processing/post-processing
                          flags: readable, writable
                          String. Default: ""
    name                : The name of the object
                          flags: readable, writable, 0x2000
                          String. Default: "gvainferencebin0"
    nireq               : Number of inference requests
                          flags: readable, writable
                          Unsigned Integer. Range: 0 - 1024 Default: 0
    no-block            : (Experimental) Option to help maintain frames per second of incoming stream. Skips inference on an incoming frame if all inference requests are currently processing outstanding frames
                          flags: readable, writable
                          Boolean. Default: false
    object-class        : Filter for Region of Interest class label on this element input
                          flags: readable, writable
                          String. Default: ""
    parent              : The parent of the object
                          flags: readable, writable, 0x2000
                          Object of type "GstObject"
    pre-process-backend : Preprocessing backend type
                          flags: readable, writable
                          Enum "GvaInferenceBinBackend" Default: 0, "auto"
                             (0): auto             - Automatic
                             (4): ie               - Inference Engine
                             (8): opencv           - OpenCV
                             (6): vaapi            - VAAPI
                             (7): vaapi-surface-sharing - VAAPI Surface Sharing
    pre-process-config  : Comma separated list of KEY=VALUE parameters for image processing pipeline configuration
                          flags: readable, writable
                          String. Default: ""
    reshape             : Enable network reshaping.  Use only 'reshape=true' without reshape-width and reshape-height properties if you want to reshape network to the original size of input frames. Note: this feature has a set of limitations. Before use, make sure that your network supports reshaping
                          flags: readable, writable
                          Boolean. Default: false
    reshape-height      : Height to which the network will be reshaped.
                          flags: readable, writable
                          Unsigned Integer. Range: 0 - 4294967295 Default: 0
    reshape-width       : Width to which the network will be reshaped.
                          flags: readable, writable
                          Unsigned Integer. Range: 0 - 4294967295 Default: 0

