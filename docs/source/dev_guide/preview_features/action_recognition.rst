Action Recognition
==================

.. attention::
  This is a preview quality feature.

To run the Action Recognition pipeline refer to the action recognition
sample located in *samples/gst_launch* folder. This feature supports
inference using following models:

* `action-recognition-0001 <https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/intel/action-recognition-0001>`__
* `driver-action-recognition-adas-0002 <https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/intel/driver-action-recognition-adas-0002>`__

To support Action Recognition we introduce new data type:

* *app/tensor* - media type agnostic custom data type used to transfer data in elements below.

.. note::
  It does not have a reliable structure and cannot be used for underlying data specification.

And the set of new elements:

* *gvavideototensor* - transforms input video into custom tensor type needed for the other elements. It also
  performs the image preprocessing such as resize, crop, color space conversion.
* *gvatensortometa* - extracts inference results from custom tensor and attaches them to the buffer as metadata.
* *gvatensorinference* - performs the inference on custom tensor with specified inference model.
* *gvatensoracc* - represents the *sliding window* algorithm. Accumulates specified number (with
  specified step) of input custom tensors into an embedding.

Infrastructure to support action recognition feature:

* *gvaactionrecognitionbin* - bin element which simplifies the pipeline
building and uses Action Recognition’s encoder and decoder models to
perform inference.

.. code-block:: none

   Pad Templates:
     SINK template: 'sink'
       Availability: Always
       Capabilities:
         video/x-raw
                    format: { (string)BGRx, (string)BGRA, (string)BGR, (string)NV12, (string)I420 }
                     width: [ 1, 2147483647 ]
                    height: [ 1, 2147483647 ]
                 framerate: [ 0/1, 2147483647/1 ]

     SRC template: 'src'
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
     SRC: 'src'
     SINK: 'sink'

   Element Properties:
     async-handling      : The bin will handle Asynchronous state changes
                           flags: readable, writable
                           Boolean. Default: false
     dec-batch-size      : Number of frames batched together for a single decoder inference. Not all models support batching. Use model optimizer to ensure that the model has batching support.
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 1024 Default: 0
     dec-device          : Decoder inference device: [CPU, GPU]
                           flags: readable, writable
                           String. Default: ""
     dec-ie-config       : Decoder's comma separated list of KEY=VALUE parameters for Inference Engine configuration
                           flags: readable, writable
                           String. Default: ""
     dec-model           : Path to decoder inference model network file
                           flags: readable, writable
                           String. Default: ""
     dec-nireq           : Decoder's number of inference requests
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 1024 Default: 1
     enc-batch-size      : Number of frames batched together for a single encoder inference. Not all models support batching. Use model optimizer to ensure that the model has batching support.
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 1024 Default: 0
     enc-device          : Encoder inference device: [CPU, GPU]
                           flags: readable, writable
                           String. Default: ""
     enc-ie-config       : Encoder's comma separated list of KEY=VALUE parameters for Inference Engine configuration
                           flags: readable, writable
                           String. Default: ""
     enc-model           : Path to encoder inference model network file
                           flags: readable, writable
                           String. Default: ""
     enc-nireq           : Encoder's number of inference requests
                           flags: readable, writable
                           Unsigned Integer. Range: 1 - 1024 Default: 1
     message-forward     : Forwards all children messages
                           flags: readable, writable
                           Boolean. Default: false
     model-proc          : Path to JSON file with description of input/output layers pre-processing/post-processing
                           flags: readable, writable
                           String. Default: ""
     name                : The name of the object
                           flags: readable, writable
                           String. Default: "gvaactionrecognitionbin0"
     parent              : The parent of the object
                           flags: readable, writable
                           Object of type "GstObject"
     pre-proc-backend    : Preprocessing backend type
                           flags: readable, writable
                           Enum "GstGVAActionRecognitionBinBackend" Default: 0, "opencv"
                              (0): opencv           - OpenCV
                              (1): ie               - Inference Engine
