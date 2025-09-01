# Performance Guide

## 1. Media and AI processing (single stream)

The Deep Learning Streamer Pipeline
Framework combines media processing with AI inference capabilities. The
simplest pipeline detects objects in a video stream stored as a disk
file.

The following command line is recommended for Intel platforms with
integrated GPU and/or NPU devices:

- `vah264dec` element uses hardware video decoder to generate output
  images (VAMemory).
- `gvadetect` element consumes VAMemory images (zero-copy operation)
  and generates inference results.
- `pre-process-backend=va` uses hardware image scaler to resize
  VAMemory image into input model tensor dimensions.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va ! queue ! gvafpscounter ! fakesink
gst-launch-1.0 filesrc location=${VIDEO_FILE} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! gvadetect model=${MODEL_FILE} device=NPU pre-process-backend=va ! queue ! gvafpscounter ! fakesink
```

When using discrete GPUs it is recommended to set
`pre-process-backend=va-surface-sharing` to enforce zero-copy
operation between video decoder and AI inference engine.

Please note `va-surface-sharing` may be slightly slower than `va`
backend on platforms with integrated GPU device. The
`va-surface-sharing` option compiles the image scaling layer into the
AI model, hence it consumes GPU compute resources.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va-surface-sharing ! queue ! gvafpscounter ! fakesink
```

While GPU device is preferred for hardware-accelerated media decoding,
it is also possible to decode video streams using CPU device. The
following table lists commands lines recommended pipelines for various
combinations of media decode and AI inference devices.

| Media Decode device | Inference device             | Sample command line                                                                                                                                                                                           |
|---------------------|------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| GPU                 | <br>GPU<br>or<br>NPU<br><br> | gst-launch-1.0 filesrc location=${VIDEO_EXAMPLE} ! parsebin ! vah264dec ! “video/x-raw(memory:VAMemory)” ! gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va ! queue ! gvafpscounter ! fakesink |
| GPU                 | CPU                          | gst-launch-1.0 filesrc location=${VIDEO_EXAMPLE} ! parsebin ! vah264dec ! “video/x-raw” ! gvadetect model=${MODEL_FILE} device=CPU pre-process-backend=opencv ! queue ! gvafpscounter ! fakesink              |
| CPU                 | <br>GPU<br>or<br>NPU<br><br> | gst-launch-1.0 filesrc location=${VIDEO_EXAMPLE} ! parsebin ! avdec_h264 ! “video/x-raw” ! gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=opencv ! queue ! gvafpscounter ! fakesink             |
| CPU                 | CPU                          | gst-launch-1.0 filesrc location=${VIDEO_EXAMPLE} ! parsebin ! avdec_h264 ! “video/x-raw” ! gvadetect model=${MODEL_FILE} device=CPU pre-process-backend=opencv ! queue ! gvafpscounter ! fakesink             |

## 2. Multi-stage pipeline with gvadetect and gvaclassify

The rules outlined above can be combined to create multi-stage
pipelines. For example, the first two inference stages can use GPU and
NPU devices with VA backend. The third element may use CPU device, after
the video stream is copied from device memory (VAMemory) to system
memory.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=GPU pre-process-backend=va ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=NPU pre-process-backend=va ! queue ! \
vapostproc ! video/x-raw ! \
gvaclassify model=${MODEL_FILE_3} device=CPU pre-process-backend=opencv ! queue ! \
gvafpscounter ! fakesink
```

Static allocation of AI stages to inference devices may be suboptimal if
one model is much bigger than others. In such case, it is recommended to
use `virtual` aggregated devices and let the OpenVINO™ inference
engine to select devices dynamically. The pre-processing backend should
be selected to handle all possible combinations.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=MULTI:GPU,NPU,CPU pre-process-backend=va ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=MULTI:GPU,NPU,CPU pre-process-backend=va ! queue ! \
gvaclassify model=${MODEL_FILE_3} device=MULTI:GPU,NPU,CPU pre-process-backend=va ! queue ! \
gvafpscounter ! fakesink
```

## 3. Multi-stream pipelines with single AI stage

The GStreamer framework can execute multiple input streams in parallel.
If streams use the same pipeline configuration, it is recommended to
create a shared inference element. The `model-instance-id=inf0`
parameter constructs such element. In addition, the `batch-size`
element should be set to the integer multiply of stream count. This
approach batches images from different streams to maximize throughput
and at the same time reduce latency penalty due to batching.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE_1} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va model-instance-id=inf0 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_2} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va model-instance-id=inf0 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_3} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va model-instance-id=inf0 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_4} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE} device=GPU pre-process-backend=va model-instance-id=inf0 batch-size=4 ! queue ! gvafpscounter ! fakesink
```

Similarly as for multi-stage scenarios, an aggregated inference device
can be used with `device=MULTI:GPU,NPU,CPU`.

Please note a single DL Streamer command line with multiple input
streams yields higher performance than running multiple DL Streamer
command lines each processioning a single single stream. The reason is
multiple command lines cannot benefit from sharing one AI model instance
and cross-stream batching.

## 4. Multi-stream pipelines with multiple AI stages

The multi-stage and multi-stream scenarios can be combined to form
complex execution graphs. In the following example four input streams
are processed by gvadetect and gvaclassify. Note the pipeline creates
only two instances of inference models:

- `inf1` with detection model running on GPU
- `inf2` with classification model running on NPU

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE_1} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=GPU pre-process-backend=va model-instance-id=inf1 batch-size=4 ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=NPU pre-process-backend=va model-instance-id=inf2 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_2} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=GPU pre-process-backend=va model-instance-id=inf1 batch-size=4 ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=NPU pre-process-backend=va model-instance-id=inf2 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_3} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=GPU pre-process-backend=va model-instance-id=inf1 batch-size=4 ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=NPU pre-process-backend=va model-instance-id=inf2 batch-size=4 ! queue ! gvafpscounter ! fakesink \
filesrc location=${VIDEO_FILE_4} ! parsebin ! vah264dec ! "video/x-raw(memory:VAMemory)" ! \
gvadetect model=${MODEL_FILE_1} device=GPU pre-process-backend=va model-instance-id=inf1 batch-size=4 ! queue ! \
gvaclassify model=${MODEL_FILE_2} device=NPU pre-process-backend=va model-instance-id=inf2 batch-size=4 ! queue ! gvafpscounter ! fakesink
```

## 5. Multi-stream pipelines with meta-aggregation element

The multi-stage and multi-stream scenarios can use the
[gvametaaggregate](../elements/gvametaaggregate.md)
element to aggregate the results from multiple branches of the pipeline.
The aggregated results are published as a single JSON metadata output.
The following example shows how to use the gvametaaggregate element to
aggregate the results from two streams pipelines.

```bash
gst-launch-1.0 filesrc location=${VIDEO_FILE_1} ! decodebin3 ! videoconvert ! \
  tee name=t t. ! queue ! gvametaaggregate name=a !
  gvaclassify model=${MODEL_FILE_2} device=CPU ! queue ! \
  gvametaconvert format=json add-tensor-data=true ! gvametapublish file-path=./result.json method=file file-format=json-lines ! \
  fakesink sync=false t. ! queue ! \
  gvadetect model=${MODEL_FILE_1} device=GPU ! a. \
  filesrc location=${VIDEO_FILE_1} ! decodebin3 ! videoconvert ! \
  gvadetect model=${MODEL_FILE_1} device=GPU ! a.
```

## 6. The Deep Learning Streamer Pipeline Framework performance benchmark results

The Deep Learning Streamer Pipeline Framework example performance benchmark
results can be found as a part of the [Smart Cities Accelerated by
Intel® Graphics Solutions
paper](https://www.intel.com/content/www/us/en/secure/content-details/826398/smart-cities-accelerated-by-intel-gpus-arc-gpu-addendum.html?wapkw=smart%20cities&DocID=826398).
