------------
Elements 2.0
------------

meta_overlay
###############

Overlays the metadata on the video frame to visualize the inference results.

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
========================      ======================================================================================================================================================================
Name                          Description
========================      ======================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

device                        Target device for meta_overlaying

                              *Default: <enum CPU device on system memory of type MetaOverlayDevice>*

========================      ======================================================================================================================================================================

object_classify
##################

Performs object classification. Accepts the ROI or full frame as an input and outputs classification results with metadata.

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
========================      ========================================================================================================================================================================================================================================================================
Name                          Description
========================      ========================================================================================================================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

model                         Path to inference model network file

                              *Default: ""*

ie-config                     Comma separated list of KEY=VALUE parameters for inference configuration

                              *Default: ""*

device                        Target device for inference. Please see inference backend documentation (ex, OpenVINO™ Toolkit) for list of supported devices.

                              *Default: CPU*

model-instance-id             Identifier for sharing resources between inference elements of the same type. Elements with the instance-id will share model and other properties. If not specified, a unique identifier will be generated.

                              *Default: ""*

nireq                         Maximum number of inference requests running in parallel

                              *Default: 0*

batch-size                    Number of frames batched together for a single inference. If the batch-size is 0, then it will be set by default to be optimal for the device. Not all models support batching. Use model optimizer to ensure that the model has batching support.

                              *Default: 0*

model-proc                    Path to JSON file with parameters describing how to build pre-process and post-process sub-pipelines

                              *Default: ""*

pre-process-backend           Preprocessing backend type

                              *Default: <enum Automatic of type VideoInferenceBackend>*

inference-interval            Run inference for every Nth frame

                              *Default: 1*

roi-inference-interval        Determines how often to run inference on each ROI object. Only valid if each ROI object has unique object id (requires object tracking after object detection)

                              *Default: 1*

inference-region              Region on which inference will be performed - full-frame or on each ROI (region of interest)bounding-box area

                              *Default: <enum Perform inference for full frame of type VideoInferenceRegion>*

object-class                  Run inference only on Region-Of-Interest with specified object class

                              *Default: ""*

labels                        Path to file containing model's output layer labels or comma separated list of KEY=VALUE pairs where KEY is name of output layer and VALUE is path to labels file. If provided, labels from model-proc won't be loaded

                              *Default: ""*

labels-file                   Path to file containing model's output layer labels. If provided, labels from model-proc won't be loaded

                              *Default: ""*

attach-tensor-data            If true, metadata will contain both post-processing results and raw tensor data. If false, metadata will contain post-processing results only.

                              *Default: True*

threshold                     Threshold for detection results. Only regions of interest with confidence values above the threshold will be added to the frame. Zero means default (auto-selected) threshold

                              *Default: 0.0*

scale-method                  Scale method to use in pre-preprocessing before inference

                              *Default: <enum Default of type VideoInferenceScaleMethod>*

repeat-metadata               If true and inference-interval > 1, metadata with last inference results will be attached to frames if inference skipped. If true and roi-inference-interval > 1, it requires object-id for each roi, so requires object tracking element inserted before this element.

                              *Default: False*

reclassify-interval           Determines how often to reclassify tracked objects. Only valid when used in conjunction with gvatrack.

                              The following values are acceptable:

                              - 0 - Do not reclassify tracked objects

                              - 1 - Always reclassify tracked objects

                              - 2:N - Tracked objects will be reclassified every N frames. Note the inference-interval is applied before determining if an object is to be reclassified (i.e. classification only occurs at a multiple of the inference interval)

                              *Default: 1*

========================      ========================================================================================================================================================================================================================================================================

object_detect
################

Performs inference-based object detection

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
========================      ========================================================================================================================================================================================================================================================================
Name                          Description
========================      ========================================================================================================================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

model                         Path to inference model network file

                              *Default: ""*

ie-config                     Comma separated list of KEY=VALUE parameters for inference configuration

                              *Default: ""*

device                        Target device for inference. Please see inference backend documentation (ex, OpenVINO™ Toolkit) for list of supported devices.

                              *Default: CPU*

model-instance-id             Identifier for sharing resources between inference elements of the same type. Elements with the instance-id will share model and other properties. If not specified, a unique identifier will be generated.

                              *Default: ""*

nireq                         Maximum number of inference requests running in parallel

                              *Default: 0*

batch-size                    Number of frames batched together for a single inference. If the batch-size is 0, then it will be set by default to be optimal for the device. Not all models support batching. Use model optimizer to ensure that the model has batching support.

                              *Default: 0*

model-proc                    Path to JSON file with parameters describing how to build pre-process and post-process sub-pipelines

                              *Default: ""*

pre-process-backend           Preprocessing backend type

                              *Default: <enum Automatic of type VideoInferenceBackend>*

inference-interval            Run inference for every Nth frame

                              *Default: 1*

roi-inference-interval        Determines how often to run inference on each ROI object. Only valid if each ROI object has unique object id (requires object tracking after object detection)

                              *Default: 1*

inference-region              Region on which inference will be performed - full-frame or on each ROI (region of interest)bounding-box area

                              *Default: <enum Perform inference for full frame of type VideoInferenceRegion>*

object-class                  Run inference only on Region-Of-Interest with specified object class

                              *Default: ""*

labels                        Path to file containing model's output layer labels or comma separated list of KEY=VALUE pairs where KEY is name of output layer and VALUE is path to labels file. If provided, labels from model-proc won't be loaded

                              *Default: ""*

labels-file                   Path to file containing model's output layer labels. If provided, labels from model-proc won't be loaded

                              *Default: ""*

attach-tensor-data            If true, metadata will contain both post-processing results and raw tensor data. If false, metadata will contain post-processing results only.

                              *Default: True*

threshold                     Threshold for detection results. Only regions of interest with confidence values above the threshold will be added to the frame. Zero means default (auto-selected) threshold

                              *Default: 0.0*

scale-method                  Scale method to use in pre-preprocessing before inference

                              *Default: <enum Default of type VideoInferenceScaleMethod>*

repeat-metadata               If true and inference-interval > 1, metadata with last inference results will be attached to frames if inference skipped. If true and roi-inference-interval > 1, it requires object-id for each roi, so requires object tracking element inserted before this element.

                              *Default: False*

========================      ========================================================================================================================================================================================================================================================================

object_track
###############

Assigns unique ID to detected objects

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
========================      ========================================================================================================================================================================================================================================================================
Name                          Description
========================      ========================================================================================================================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

model                         Path to inference model network file

                              *Default: ""*

ie-config                     Comma separated list of KEY=VALUE parameters for inference configuration

                              *Default: ""*

device                        Target device for inference. Please see inference backend documentation (ex, OpenVINO™ Toolkit) for list of supported devices.

                              *Default: CPU*

model-instance-id             Identifier for sharing resources between inference elements of the same type. Elements with the instance-id will share model and other properties. If not specified, a unique identifier will be generated.

                              *Default: ""*

nireq                         Maximum number of inference requests running in parallel

                              *Default: 0*

batch-size                    Number of frames batched together for a single inference. If the batch-size is 0, then it will be set by default to be optimal for the device. Not all models support batching. Use model optimizer to ensure that the model has batching support.

                              *Default: 0*

model-proc                    Path to JSON file with parameters describing how to build pre-process and post-process sub-pipelines

                              *Default: ""*

pre-process-backend           Preprocessing backend type

                              *Default: <enum Automatic of type VideoInferenceBackend>*

inference-interval            Run inference for every Nth frame

                              *Default: 1*

roi-inference-interval        Determines how often to run inference on each ROI object. Only valid if each ROI object has unique object id (requires object tracking after object detection)

                              *Default: 1*

inference-region              Region on which inference will be performed - full-frame or on each ROI (region of interest)bounding-box area

                              *Default: <enum Perform inference for full frame of type VideoInferenceRegion>*

object-class                  Run inference only on Region-Of-Interest with specified object class

                              *Default: ""*

labels                        Path to file containing model's output layer labels or comma separated list of KEY=VALUE pairs where KEY is name of output layer and VALUE is path to labels file. If provided, labels from model-proc won't be loaded

                              *Default: ""*

labels-file                   Path to file containing model's output layer labels. If provided, labels from model-proc won't be loaded

                              *Default: ""*

attach-tensor-data            If true, metadata will contain both post-processing results and raw tensor data. If false, metadata will contain post-processing results only.

                              *Default: True*

threshold                     Threshold for detection results. Only regions of interest with confidence values above the threshold will be added to the frame. Zero means default (auto-selected) threshold

                              *Default: 0.0*

scale-method                  Scale method to use in pre-preprocessing before inference

                              *Default: <enum Default of type VideoInferenceScaleMethod>*

repeat-metadata               If true and inference-interval > 1, metadata with last inference results will be attached to frames if inference skipped. If true and roi-inference-interval > 1, it requires object-id for each roi, so requires object tracking element inserted before this element.

                              *Default: False*

generate-objects              If true, generate objects (according to previous trajectory) if not detected on current frame

                              *Default: True*

adjust-objects                If true, adjust object position for more smooth trajectory

                              *Default: True*

tracking-per-class            If true, object association takes into account object class

                              *Default: False*

spatial-feature               Spatial feature used by object tracking algorithm

                              *Default: <enum Spatial feature not used (only temporal features used, such as object shape and trajectory) of type SpatialFeatureType>*

spatial-feature-distance      Method to calculate distance between two spatial features

                              *Default: <enum Spatial feature not used of type SpatialFeatureDistanceType>*

tracking-type                 DEPRECATED - use other properties according to the following mapping:

                              zero-term-imageless:  generate-objects=false adjust-objects=false spatial-feature=none

                              zero-term:            generate-objects=false adjust-objects=false spatial-feature=sliced-histogram

                              short-term-imageless: generate-objects=true  adjust-objects=false spatial-feature=none

                              short-term:           generate-objects=true  adjust-objects=false spatial-feature=sliced-histogram

                              *Default: ""*

========================      ========================================================================================================================================================================================================================================================================

processbin
#############

Bin element for processing pipelines using branching: tee name=t t. ! <preprocess> ! <process> ! <postprocess> ! <aggregate>  t. ! aggregate

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
========================      ======================================================================================================================================================================
Name                          Description
========================      ======================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

========================      ======================================================================================================================================================================

video_inference
##################

Runs Deep Learning inference on any model with RGB-like input

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
========================      ========================================================================================================================================================================================================================================================================
Name                          Description
========================      ========================================================================================================================================================================================================================================================================
name                          The name of the object

                              *Default: None*

parent                        The parent of the object

                              *Default: None*

async-handling                The bin will handle Asynchronous state changes

                              *Default: False*

message-forward               Forwards all children messages

                              *Default: False*

preprocess                    Pre-processing element

                              *Default: None*

process                       Main processing element

                              *Default: None*

postprocess                   Post-processing element

                              *Default: None*

aggregate                     (Optional) Element to aggregate preprocess/process/postprocess result and original frame

                              *Default: None*

postaggregate                 (Optional) Element inserted after aggregation element

                              *Default: None*

preprocess-queue-size         Size of queue (in number buffers) before pre-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

process-queue-size            Size of queue (in number buffers) before processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postprocess-queue-size        Size of queue (in number buffers) before post-processing element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

aggregate-queue-size          Size of queue (in number buffers) for original frames between 'tee' and aggregate element. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

postaggregate-queue-size      Size of queue (in number buffers) between aggregate and post-aggregate elements. Special values: -1 means no queue element, 0 means queue of unlimited size

                              *Default: 0*

model                         Path to inference model network file

                              *Default: ""*

ie-config                     Comma separated list of KEY=VALUE parameters for inference configuration

                              *Default: ""*

device                        Target device for inference. Please see inference backend documentation (ex, OpenVINO™ Toolkit) for list of supported devices.

                              *Default: CPU*

model-instance-id             Identifier for sharing resources between inference elements of the same type. Elements with the instance-id will share model and other properties. If not specified, a unique identifier will be generated.

                              *Default: ""*

nireq                         Maximum number of inference requests running in parallel

                              *Default: 0*

batch-size                    Number of frames batched together for a single inference. If the batch-size is 0, then it will be set by default to be optimal for the device. Not all models support batching. Use model optimizer to ensure that the model has batching support.

                              *Default: 0*

model-proc                    Path to JSON file with parameters describing how to build pre-process and post-process sub-pipelines

                              *Default: ""*

pre-process-backend           Preprocessing backend type

                              *Default: <enum Automatic of type VideoInferenceBackend>*

inference-interval            Run inference for every Nth frame

                              *Default: 1*

roi-inference-interval        Determines how often to run inference on each ROI object. Only valid if each ROI object has unique object id (requires object tracking after object detection)

                              *Default: 1*

inference-region              Region on which inference will be performed - full-frame or on each ROI (region of interest)bounding-box area

                              *Default: <enum Perform inference for full frame of type VideoInferenceRegion>*

object-class                  Run inference only on Region-Of-Interest with specified object class

                              *Default: ""*

labels                        Path to file containing model's output layer labels or comma separated list of KEY=VALUE pairs where KEY is name of output layer and VALUE is path to labels file. If provided, labels from model-proc won't be loaded

                              *Default: ""*

labels-file                   Path to file containing model's output layer labels. If provided, labels from model-proc won't be loaded

                              *Default: ""*

attach-tensor-data            If true, metadata will contain both post-processing results and raw tensor data. If false, metadata will contain post-processing results only.

                              *Default: True*

threshold                     Threshold for detection results. Only regions of interest with confidence values above the threshold will be added to the frame. Zero means default (auto-selected) threshold

                              *Default: 0.0*

scale-method                  Scale method to use in pre-preprocessing before inference

                              *Default: <enum Default of type VideoInferenceScaleMethod>*

repeat-metadata               If true and inference-interval > 1, metadata with last inference results will be attached to frames if inference skipped. If true and roi-inference-interval > 1, it requires object-id for each roi, so requires object tracking element inserted before this element.

                              *Default: False*

========================      ========================================================================================================================================================================================================================================================================

batch_create
###############

Accumulate multiple buffers into single buffer with multiple GstMemory

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
SINK template: sink      - Availability: Always
===================      ======================



Properties
**********************
==========      ===================================
Name            Description
==========      ===================================
name            The name of the object

                *Default: None*

parent          The parent of the object

                *Default: None*

qos             Handle Quality-of-Service events

                *Default: False*

batch-size      Number of frames to batch together

                *Default: 1*

==========      ===================================

batch_split
##############

Split input tensor (remove batch dimension from tensor shape)

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
SINK template: sink      - Availability: Always
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

capsrelax
############

Pass data without modification, relaxes formats

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

gvadrop
##########

Pass / drop custom number of frames in pipeline

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
===========      ==============================================
Name             Description
===========      ==============================================
name             The name of the object

                 *Default: None*

parent           The parent of the object

                 *Default: None*

qos              Handle Quality-of-Service events

                 *Default: False*

pass-frames      Number of frames to pass along the pipeline.

                 *Default: 1*

drop-frames      Number of frames to drop.

                 *Default: 0*

mode             Mode defines what to do with dropped frames

                 *Default: <enum Default of type GvaDropMode>*

===========      ==============================================

meta_aggregate
#################

Muxes video streams with tensor's ROI into single stream

Capabilities
**********************
========================      ==========================
SINK template: meta_%u        - Availability: On request
                              - Capabilities:
                              
                                - video/x-raw
                                 
SINK template: tensor_%u      - Availability: On request
                              - Capabilities:
                              
                                - other/tensors
                                 
SINK template: sink           - Availability: Always
SRC template: src             - Availability: Always
========================      ==========================



Properties
**********************
====================      ===========================================================================================================================================================================================================================================================================================================
Name                      Description
====================      ===========================================================================================================================================================================================================================================================================================================
name                      The name of the object

                          *Default: None*

parent                    The parent of the object

                          *Default: None*

latency                   Additional latency in live mode to allow upstream to take longer to produce buffers for the current position (in nanoseconds)

                          *Default: 0*

min-upstream-latency      When sources with a higher latency are expected to be plugged in dynamically after the aggregator has started playing, this allows overriding the minimum latency reported by the initial source(s). This is only taken into account when larger than the actually reported minimum latency. (nanoseconds)

                          *Default: 0*

start-time-selection      Decides which start time is output

                          *Default: <enum GST_AGGREGATOR_START_TIME_SELECTION_ZERO of type GstAggregatorStartTimeSelection>*

start-time                Start time to use if start-time-selection=set

                          *Default: 18446744073709551615*

emit-signals              Send signals

                          *Default: False*

attach-tensor-data        If true, additionally copies tensor data into metadata

                          *Default: True*

====================      ===========================================================================================================================================================================================================================================================================================================

meta_smooth
##############

smooth metadata

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
SINK template: sink      - Availability: Always
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

roi_split
############

Split buffer with multiple GstVideoRegionOfInterestMeta into multiple buffers

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
SINK template: sink      - Availability: Always
===================      ======================



Properties
**********************
============      =========================================================================================================================
Name              Description
============      =========================================================================================================================
name              The name of the object

                  *Default: None*

parent            The parent of the object

                  *Default: None*

qos               Handle Quality-of-Service events

                  *Default: False*

object-class      Filter ROI list by object class(es) (comma separated list if multiple). Output only ROIs with specified object class(es)

                  *Default: ""*

============      =========================================================================================================================

video_frames_buffer
######################

Buffer and optionally repeat compressed video frames

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
SINK template: sink      - Availability: Always
===================      ======================



Properties
**********************
=================      ========================================
Name                   Description
=================      ========================================
name                   The name of the object

                       *Default: None*

parent                 The parent of the object

                       *Default: None*

qos                    Handle Quality-of-Service events

                       *Default: False*

num-input-frames       Number input frames to buffer

                       *Default: 0*

num-output-frames      Max number output frames in 'loop' mode

                       *Default: 0*

=================      ========================================

rate_adjust
##############

Adjust frame rate. Output frame rate is input rate multiplied by (numerator/denominator)

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
======      =================================================================================================================
Name        Description
======      =================================================================================================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

ratio       Frame rate ratio - output frame rate is input rate multiplied by specified ratio. Current limitation: ratio <= 1

            *Default: None*

======      =================================================================================================================

tensor_convert
#################

Convert (zero-copy if possible) between video/audio and tensors media type

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBP
                           - video/x-raw
                            
                             - format: BGRP
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: uint8
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

tensor_histogram
###################

Calculates histogram on tensors of UInt8 data type and NHWC layout

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: uint8
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: float32
===================      ======================



Properties
**********************
============      =============================================================================================================================================================
Name              Description
============      =============================================================================================================================================================
name              The name of the object

                  *Default: None*

parent            The parent of the object

                  *Default: None*

qos               Handle Quality-of-Service events

                  *Default: False*

width             Input tensor width, assuming tensor in NHWC or NCHW layout

                  *Default: 64*

height            Input tensor height, assuming tensor in NHWC or NCHW layout

                  *Default: 64*

num-slices-x      Number slices along X-axis

                  *Default: 1*

num-slices-y      Number slices along Y-axis

                  *Default: 1*

num-bins          Number bins in histogram calculation. Example, for 3-channel tensor (RGB image), output histogram size is equal to (num_bin^3 * num_slices_x * num_slices_y)

                  *Default: 8*

batch-size        Batch size

                  *Default: 1*

device            CPU or GPU or GPU.0, GPU.1, ..

                  *Default: ""*

============      =============================================================================================================================================================

tensor_postproc_add_params
#############################

Post-processing to only add properties/parameters to metadata

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==============      =======================================================
Name                Description
==============      =======================================================
name                The name of the object

                    *Default: None*

parent              The parent of the object

                    *Default: None*

qos                 Handle Quality-of-Service events

                    *Default: False*

attribute-name      Name for metadata created and attached by this element

                    *Default: attribute*

format              Format description

                    *Default: ""*

==============      =======================================================

tensor_postproc_detection
############################

Post-processing of object detection inference to extract bounding box coordinates, confidence, label, mask

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
=================      =========================================================================================================
Name                   Description
=================      =========================================================================================================
name                   The name of the object

                       *Default: None*

parent                 The parent of the object

                       *Default: None*

qos                    Handle Quality-of-Service events

                       *Default: False*

labels                 Array of object classes

                       *Default: None*

labels-file            Path to .txt file containing object classes (one per line)

                       *Default: ""*

threshold              Detection threshold - only objects with confidence values above the threshold will be added to the frame

                       *Default: 0.5*

box-index              Index of layer containing bounding box data

                       *Default: -1*

confidence-index       Index of layer containing confidence data

                       *Default: -1*

label-index            Index of layer containing label data

                       *Default: -1*

imageid-index          Index of layer containing imageid data

                       *Default: -1*

mask-index             Index of layer containing mask data

                       *Default: -1*

box-offset             Offset inside layer containing bounding box data

                       *Default: -1*

confidence-offset      Offset inside layer containing confidence data

                       *Default: -1*

label-offset           Offset inside layer containing label data

                       *Default: -1*

imageid-offset         Offset inside layer containing imageid data

                       *Default: -1*

=================      =========================================================================================================

tensor_postproc_label
########################

Post-processing of classification inference to extract object classes

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==================      =====================================================================
Name                    Description
==================      =====================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

method                  Method used to post-process tensor data

                        *Default: <enum max of type method>*

labels                  Array of object classes

                        *Default: None*

labels-file             Path to .txt file containing object classes (one per line)

                        *Default: ""*

attribute-name          Name for metadata created and attached by this element

                        *Default: ""*

layer-name              Name of output layer to process (in case of multiple output tensors)

                        *Default: ""*

threshold               Threshold for confidence values

                        *Default: 0.0*

compound-threshold      Threshold for compound method

                        *Default: 0.5*

==================      =====================================================================

tensor_postproc_text
#######################

Post-processing to convert tensor data into text

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==============      =====================================================================
Name                Description
==============      =====================================================================
name                The name of the object

                    *Default: None*

parent              The parent of the object

                    *Default: None*

qos                 Handle Quality-of-Service events

                    *Default: False*

text-scale          Scale tensor values before converting to text

                    *Default: 1.0*

text-precision      Precision for floating-point to text conversion

                    *Default: 0*

attribute-name      Name for metadata created and attached by this element

                    *Default: ""*

layer-name          Name of output layer to process (in case of multiple output tensors)

                    *Default: ""*

==============      =====================================================================

tensor_postproc_yolo
#######################

Post-processing of YOLO models to extract bounding box list

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
=========================      ========================================================================================================
Name                           Description
=========================      ========================================================================================================
name                           The name of the object

                               *Default: None*

parent                         The parent of the object

                               *Default: None*

qos                            Handle Quality-of-Service events

                               *Default: False*

version                        Yolo's version number. Supported only from 3 to 5

                               *Default: 0*

labels                         Array of object classes

                               *Default: None*

labels-file                    Path to .txt file containing object classes (one per line)

                               *Default: ""*

threshold                      Detection threshold - only objects with confidence value above the threshold will be added to the frame

                               *Default: 0.5*

anchors                        Anchor values array

                               *Default: None*

masks                          Masks values array (1 dimension)

                               *Default: None*

iou-threshold                  IntersectionOverUnion threshold

                               *Default: 0.5*

do-cls-softmax                 If true, perform softmax

                               *Default: True*

output-sigmoid-activation      output_sigmoid_activation

                               *Default: True*

cells-number                   Number of cells. Use if number of cells along x and y axes is the same (0 = autodetection)

                               *Default: 0*

cells-number-x                 Number of cells along x-axis

                               *Default: 0*

cells-number-y                 Number of cells along y-axis

                               *Default: 0*

bbox-number-on-cell            Number of bounding boxes that can be predicted per cell (0 = autodetection)

                               *Default: 0*

classes                        Number of classes

                               *Default: 0*

nms                            Apply Non-Maximum Suppression (NMS) filter to bounding boxes

                               *Default: True*

=========================      ========================================================================================================

tensor_sliding_window
########################

Sliding aggregation of input tensors

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

openvino_tensor_inference
############################

Inference on OpenVINO™ toolkit backend

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==================      ======================================================================================================================
Name                    Description
==================      ======================================================================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

model                   Path to model file in OpenVINO™ toolkit or ONNX format

                        *Default: ""*

device                  Target device for inference. Please see OpenVINO™ toolkit documentation for list of supported devices.

                        *Default: CPU*

config                  Comma separated list of KEY=VALUE parameters for Inference Engine configuration

                        *Default: ""*

batch-size              Batch size

                        *Default: 1*

buffer-pool-size        Output buffer pool size (functionally same as OpenVINO™ toolkit nireq parameter)

                        *Default: 16*

shared-instance-id      Identifier for sharing backend instance between multiple elements, for example in elements processing multiple inputs

                        *Default: ""*

==================      ======================================================================================================================

openvino_video_inference
###########################

Inference on OpenVINO™ toolkit backend

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: NV12
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==================      ======================================================================================================================
Name                    Description
==================      ======================================================================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

model                   Path to model file in OpenVINO™ toolkit or ONNX format

                        *Default: ""*

device                  Target device for inference. Please see OpenVINO™ toolkit documentation for list of supported devices.

                        *Default: CPU*

config                  Comma separated list of KEY=VALUE parameters for Inference Engine configuration

                        *Default: ""*

batch-size              Batch size

                        *Default: 1*

buffer-pool-size        Output buffer pool size (functionally same as OpenVINO™ toolkit nireq parameter)

                        *Default: 16*

shared-instance-id      Identifier for sharing backend instance between multiple elements, for example in elements processing multiple inputs

                        *Default: ""*

==================      ======================================================================================================================

opencv_cropscale
###################

Fused video crop and scale on OpenCV backend. Crop operation supports GstVideoCropMeta if attached to input buffer

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
===================      ======================



Properties
**********************
===========      ==================================================
Name             Description
===========      ==================================================
name             The name of the object

                 *Default: None*

parent           The parent of the object

                 *Default: None*

qos              Handle Quality-of-Service events

                 *Default: False*

add-borders      Add borders if necessary to keep the aspect ratio

                 *Default: False*

===========      ==================================================

opencv_find_contours
#######################

Find contour points of given mask using opencv

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
=====================      ===============================================================================================================
Name                       Description
=====================      ===============================================================================================================
name                       The name of the object

                           *Default: None*

parent                     The parent of the object

                           *Default: None*

qos                        Handle Quality-of-Service events

                           *Default: False*

mask-metadata-name         Name of metadata containing segmentation mask

                           *Default: mask*

contour-metadata-name      Name of metadata created by this element to store contour(s)

                           *Default: contour*

threshold                  Mask threshold - only mask pixels with confidence values above the threshold will be used for finding contours

                           *Default: 0.5*

=====================      ===============================================================================================================

opencv_meta_overlay
######################

Visualize inference results using OpenCV

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
===================      ======================



Properties
**********************
=================      =================================================
Name                   Description
=================      =================================================
name                   The name of the object

                       *Default: None*

parent                 The parent of the object

                       *Default: None*

qos                    Handle Quality-of-Service events

                       *Default: False*

lines-thickness        Thickness of lines and rectangles

                       *Default: 2*

font-thickness         Font thickness

                       *Default: 1*

font-scale             Font scale

                       *Default: 1.0*

attach-label-mask      Attach label mask as metadata, image not changed

                       *Default: False*

=================      =================================================

opencv_object_association
############################

Assigns unique ID to ROI objects based on objects trajectory and (optionally) feature vector obtained from ROI metadata

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
SRC template: src        - Availability: Always
===================      ======================



Properties
**********************
=============================      ==============================================================================================
Name                               Description
=============================      ==============================================================================================
name                               The name of the object

                                   *Default: None*

parent                             The parent of the object

                                   *Default: None*

qos                                Handle Quality-of-Service events

                                   *Default: False*

generate-objects                   If true, generate objects (according to previous trajectory) if not detected on current frame

                                   *Default: True*

adjust-objects                     If true, adjust object position for more smooth trajectory

                                   *Default: True*

tracking-per-class                 If true, object association takes into account object class

                                   *Default: False*

spatial-feature-metadata-name      Name of metadata containing spatial feature

                                   *Default: spatial-feature*

spatial-feature-distance           Method to calculate distance between two spatial features

                                   *Default: <enum bhattacharyya of type spatial-feature-distance>*

shape-feature-weight               Weighting factor for shape-based feature

                                   *Default: 0.75*

trajectory-feature-weight          Weighting factor for trajectory-based feature

                                   *Default: 0.5*

spatial-feature-weight             Weighting factor for spatial feature

                                   *Default: 0.25*

min-region-ratio-in-boundary        Min region ratio in image boundary

                                   *Default: 0.75*

=============================      ==============================================================================================

opencv_remove_background
###########################

Remove background using mask

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
===================      ======================



Properties
**********************
==================      ===================================================================================================================
Name                    Description
==================      ===================================================================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

mask-metadata-name      Name of metadata containing segmentation mask

                        *Default: mask*

threshold               Mask threshold - only mask pixels with confidence values above the threshold will be used for setting transparency

                        *Default: 0.5*

==================      ===================================================================================================================

opencv_tensor_normalize
##########################

Convert U8 tensor to F32 tensor with normalization

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: uint8
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: float32
===================      ======================



Properties
**********************
======      ====================================================================
Name        Description
======      ====================================================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

range       Normalization range MIN, MAX. Example: <0,1>

            *Default: None*

mean        Mean values per channel. Example: <0.485,0.456,0.406>

            *Default: None*

std         Standard deviation values per channel. Example: <0.229,0.224,0.225>

            *Default: None*

======      ====================================================================

opencv_warp_affine
#####################

Rotation using cv::warpAffine

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: RGB
                           - video/x-raw
                            
                             - format: BGR
                           - video/x-raw
                            
                             - format: RGBA
                           - video/x-raw
                            
                             - format: BGRA
===================      ======================



Properties
**********************
======      ===================================================================
Name        Description
======      ===================================================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

angle       Angle by which the picture is rotated (in radians)

            *Default: 0.0*

sync        Wait for OpenCL kernel completion (if running on GPU via cv::UMat)

            *Default: False*

======      ===================================================================

tensor_postproc_human_pose
#############################

Post-processing to extract key points from human pose estimation model output

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
=================      =====================================================================
Name                   Description
=================      =====================================================================
name                   The name of the object

                       *Default: None*

parent                 The parent of the object

                       *Default: None*

qos                    Handle Quality-of-Service events

                       *Default: False*

point-names            Array of key point names

                       *Default: None*

point-connections      Array of point connections {name-A0, name-B0, name-A1, name-B1, ...}

                       *Default: None*

=================      =====================================================================

vaapi_batch_proc
###################

Batched pre-processing with VAAPI memory as input and output

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==================      ======================================================================================================================
Name                    Description
==================      ======================================================================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

add-borders             Add borders if necessary to keep the aspect ratio

                        *Default: False*

output-format           Image format for output frames: BGR or RGB or GRAY

                        *Default: BGR*

shared-instance-id      Identifier for sharing backend instance between multiple elements, for example in elements processing multiple inputs

                        *Default: ""*

==================      ======================================================================================================================

vaapi_sync
#############

Synchronize VAAPI surfaces (call vaSyncSurface)

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
===================      ======================



Properties
**********************
=======      ==================================
Name         Description
=======      ==================================
name         The name of the object

             *Default: None*

parent       The parent of the object

             *Default: None*

qos          Handle Quality-of-Service events

             *Default: False*

timeout      Synchronization timeout (seconds)

             *Default: 10.0*

=======      ==================================

opencl_tensor_normalize
##########################

Convert U8 tensor to U8 or F32 tensor with normalization

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
==================      ======================================================================================================================
Name                    Description
==================      ======================================================================================================================
name                    The name of the object

                        *Default: None*

parent                  The parent of the object

                        *Default: None*

qos                     Handle Quality-of-Service events

                        *Default: False*

shared-instance-id      Identifier for sharing backend instance between multiple elements, for example in elements processing multiple inputs

                        *Default: ""*

==================      ======================================================================================================================

vaapi_to_opencl
##################

Convert memory:VASurface to memory:OpenCL

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                           - other/tensors
                            
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

======      =================================

sycl_meta_overlay
####################

Visualize inference results using DPC++/SYCL backend

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - video/x-raw
                            
                             - format: BGRA
                           - video/x-raw
                            
                             - format: RGBA
===================      ======================



Properties
**********************
===============      ==================================
Name                 Description
===============      ==================================
name                 The name of the object

                     *Default: None*

parent               The parent of the object

                     *Default: None*

qos                  Handle Quality-of-Service events

                     *Default: False*

lines-thickness      Thickness of lines and rectangles

                     *Default: 2*

===============      ==================================

sycl_tensor_histogram
########################

Calculates histogram on tensors of UInt8 data type and NHWC layout

Capabilities
**********************
===================      ======================
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: uint8
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: uint8
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
                             - num_tensors: 1
                             - types: float32
===================      ======================



Properties
**********************
============      =============================================================================================================================================================
Name              Description
============      =============================================================================================================================================================
name              The name of the object

                  *Default: None*

parent            The parent of the object

                  *Default: None*

qos               Handle Quality-of-Service events

                  *Default: False*

width             Input tensor width, assuming tensor in NHWC or NCHW layout

                  *Default: 64*

height            Input tensor height, assuming tensor in NHWC or NCHW layout

                  *Default: 64*

num-slices-x      Number slices along X-axis

                  *Default: 1*

num-slices-y      Number slices along Y-axis

                  *Default: 1*

num-bins          Number bins in histogram calculation. Example, for 3-channel tensor (RGB image), output histogram size is equal to (num_bin^3 * num_slices_x * num_slices_y)

                  *Default: 8*

batch-size        Batch size

                  *Default: 1*

device            CPU or GPU or GPU.0, GPU.1, ..

                  *Default: ""*

============      =============================================================================================================================================================

inference_openvino
#####################

OpenVINO™ toolkit inference element

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
======      =================================
Name        Description
======      =================================
name        The name of the object

            *Default: None*

parent      The parent of the object

            *Default: None*

qos         Handle Quality-of-Service events

            *Default: False*

device      Inference device

            *Default: CPU*

model       OpenVINO™ toolkit model path

            *Default: ""*

nireq       Number inference requests

            *Default: 0*

======      =================================

pytorch_tensor_inference
###########################

PyTorch inference element

Capabilities
**********************
===================      ======================
SRC template: src        - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
SINK template: sink      - Availability: Always
                         - Capabilities:
                         
                           - other/tensors
                            
===================      ======================



Properties
**********************
=============      ===================================================================================================================================================
Name               Description
=============      ===================================================================================================================================================
name               The name of the object

                   *Default: None*

parent             The parent of the object

                   *Default: None*

qos                Handle Quality-of-Service events

                   *Default: False*

device             Inference device

                   *Default: cpu*

model              The full module name of the PyTorch model to be imported from torchvision or model path. Ex. 'torchvision.models.resnet50' or '/path/to/model.pth'

                   *Default: ""*

model-weights      PyTorch model weights path. If model-weights is empty, the default weights will be used

                   *Default: ""*

=============      ===================================================================================================================================================

