# How to Create Model-proc File

In this tutorial you will learn how to create model-proc file for your
own CNN model that can be processed by Deep Learning Streamer
Pipeline Framework.

Please refer to the [model-proc documentation](./model_proc_file.md)
before going through this tutorial.

## Content

- [Theory](#theory)
  - [When do you need to specify model-proc file?](#when-do-you-need-to-specify-model-proc-file)
  - [How to define pre-processing](#how-to-define-pre-processing)
  - [How to define post-processing](#how-to-define-post-processing)
- [Practice](#practice)
  - [Build model-proc for classification model with advance pre-processing](#build-model-proc-for-classification-model-with-advance-pre-processing)
  - [Build model-proc for detection model with advance post-processing](#build-model-proc-for-detection-model-with-advance-post-processing)

## Theory

### When do you need to specify model-proc file?

To answer this question, you need to answer the following:

1. Does the model have one input layer?
2. Is one image resize enough as a pre-processing?
3. Does the model have one output layer?
4. Is the default post-processing suitable for the output layer type of
   the model? For more details, refer to the section about the
   [default behavior](./model_proc_file.md#post-processing-configuration).
5. Is it necessary to specify labels so that the post-processor uses
   this information and adds it to the classification or detection
   results?

If at least one question from the list above is answered in the
negative, it is mandatory to determine the model-proc file.

If the answer is negative only for items 1-2, you need to define the
field *"input_preproc"*. How to do this is described in the section
[How to define pre-processing](#how-to-define-pre-processing).

If the answer is negative only for items 3-5, you need to define the
field *"output_postproc"*. How to do this is described in the section
[How to define post-processing](#how-to-define-post-processing)

## How to define pre-processing

### Model has several input layers

The general case when the model has 2 or more input layers is **not
supported** by Pipeline Framework, however, there is an **exception**:

1. The model requires an image as an input for only one layer;
2. The second layer is a layer of the following formats:
   1. *"image_info"* - format: *B, C*, where:

      - *B* - batch size
      - *C* - vector of 3 values in format *H, W, S*, where *H* is
        an image height, *W* is an image width, *S* is an image
        scale factor (usually 1).

      You can specify only *S* parameter.

   2. *"sequence_index"* - Set blob for this layer to *[1, 1, 1,..., 1]*.

In the table below you can find examples of model-proc files that use
formats described above:

| Model | Model-proc | 2nd layer format |
|---|---|---|
| [Faster-RCNN](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/faster_rcnn_resnet50_coco) | [preproc-image-info.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/public/preproc-image-info.json) | image_info |
| [license-plate-recognition-barrier-0007](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/license-plate-recognition-barrier-0007) | [license-plate-recognition-barrier-0007.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/intel/license-plate-recognition-barrier-0007.json) | sequence_index |

### Model requires more advance image pre-processing algorithm then resize without aspect-ratio preservation

In the simplest case, one resize is enough for the model inference to be
successful. However, if the goal is to get the highest possible
accuracy, this may not be enough.

*OpenCV pre-process-backend* supports follow operations:

1. *resize*
2. *color_space*
3. *normalization*
4. *padding*

In the table below you can find examples of model-proc files that use
some of the operations described above:

| Model | Model-proc | Operation |
|---|---|---|
| [MobileNet](https://github.com/onnx/models/blob/main/validated/vision/classification/mobilenet) | [mobilenetv2-7.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/onnx/mobilenetv2-7.json)   | normalization |
| [single-human-pose-estimation-0001](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/single-human-pose-estimation-0001) | [single-human-pose-estimation-0001.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/public/single-human-pose-estimation-0001.json)  | padding |

For details see
[model-proc documentation](./model_proc_file.md).

## How to define post-processing

### Model has several output layers

If the model has several output layers, each of them should have a
converter in *"output_postproc"* for separate processing. Example:

| Model | Model-proc |
|---|---|
| [age-gender-recognition-retail-0013](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/intel/age-gender-recognition-retail-0013)   |[age-gender-recognition-retail-0013.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/intel/age-gender-recognition-retail-0013.json) |

For joint processing of blobs from several output layers, it is enough
to specify only one converter and the field *"layer_names": ["layer_name_1", .. , "layer_name_n"]* in it.

Example:

| Model | Model-proc |
|---|---|
| [YOLOv3](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/yolo-v3-tf) |[yolo-v3-tf.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/public/yolo-v3-tf.json) |

> **NOTE:** In this example, you will not find the use of the *"layer_names"*
> field, because it is not necessary to specify it in the case when the
> converter expects the same number of outputs as the model has.

### Output blob's shape is not appropriate for default converter

In this case in *"output_postproc"* it's necessary to list the
description of converters for each of the output layer (or list of
layers) that requires processing, with an explicit indication of the
type of converter. See the examples from the previous sections.

To determine which converter is suitable in your case, please refer to
the [documentation](./model_proc_file.md).

> **NOTE:** If there is no suitable converter among the listed converters, there are
> several ways to add the necessary processing. For more information, see
> [Custom Processing section](./custom_processing.md)

### Need to have *labels* information

The information about labels can be provided in two ways:

- via *labels* property of inference elements
- via model-proc file

The *labels* property is a convenient way to provide information about
labels. It takes the path to a file where each label starts with a new
line.

To specify labels in a model-proc file, you need to define the converter
and specify *"labels"* field as a list or a path to labels file.

> **NOTE:** The *labels* property takes precedence over labels specified in a
> model-proc file.

Examples of labels in model-proc files:

| Dataset | Model | Model-proc |
|---|---|---|
| ImageNet | [resnet-18-pytorch](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/resnet-18-pytorch) | [preproc-aspect-ratio.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/public/preproc-aspect-ratio.json) |
| COCO | [YOLOv2](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/yolo-v2-tf) |[yolo-v2-tf.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/public/yolo-v2-tf.json) |
| PASCAL VOC | [yolo-v2-ava-0001](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/intel/yolo-v2-ava-0001) | [yolo-v2-ava-0001.json](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/model_proc/intel/yolo-v2-ava-0001.json) |

## Practice

### Build model-proc for classification model with advance pre-processing

In this section, we will learn how to build a model-proc file for a
model
[SqueezeNet v1.1](https://docs.openvino.ai/2023.3/omz_models_model_squeezenet1_1.html).
Let's start with an empty template:

``` javascript
// squeezenet1.1.json
{
    "json_schema_version": "2.2.0",
    "input_preproc": [],
    "output_postproc": []
}
```

#### Defining "input_preproc"

This model is trained on the ImageNet dataset. The standard
pre-processing when training models on this dataset is *resize with
aspect-ratio* preservation. Also, the input channels of the *RGB* image
are normalized according to a given distribution *mean: [0.485, 0.456,
0.406], std: [0.229, 0.224, 0.225]*. However, similar operations are
added when converting the model to Intermediate Representation. It is
worth noting that trained models usually accept an

*RGB* image as input, while the Inference Engine requires *BGR* as
input. And the *RGB -> BGR* conversion is also an IR model
pre-processing operation.

> **NOTE:** If you are going to use the ONNX model, you need to add these operations
> to *"input_preproc"* yourself.

If you are in doubt about which pre-processing is necessary, then
contact the creator of the model. If the model is represented by OMZ,
refer to the documentation. A config file for the Accuracy Checker tool
can also help. Usually, it is located in the folder with the description
of the model.

``` javascript
"input_preproc": [
    "format": "image",
    "layer_name": "data", // <input value="data"/> field in the end of .xml (<meta_data> section)
    "params": {
        "resize": "aspect-ratio"
    }
]
```

So, *"input_preproc"* is defined.

> **NOTE:** For ONNX model *"input_preproc"* most likely would be the following:

``` javascript
"input_preproc": [
    "format": "image",
    "layer_name": "data",
    "precision": "FP32", // because onnx model usually requires pixels in [0, 1] range
    "params": {
        "color_space": "RGB",
        "resize": "aspect-ratio",
        "range": [0.0, 1.0],
        "mean": [0.485, 0.456, 0.406],
        "std": [0.229, 0.224, 0.225]
    }
]
```

> **NOTE:** Such a configurable pre-processing can be executed only with OpenCV
> *pre-process-backend*. To improve performance, you can leave
> "input_preproc" empty (*"input_preproc": []*), then resize without
> aspect-ratio will be performed by any of the *pre-process-backend*.
> However, this may affect the accuracy of the model inference.

#### Defining "output_postproc"

This model has a single output layer *\<output value="['prob']"/>*
field in the end of .xml (section)), so
field *"layer_name": "prob"* is optional. For this model *label*
with *max* method is suitable converter.

Also if you want to see results with labels you should set *"labels"*
field. They also can be put into a separate file to keep model-proc file
small in size.

Alternatively, you can specify labels using the *labels* property of
inference elements. In this case, you don't need to add the
*"labels"* field to the model-proc file.

> **NOTE:** Because ImageNet's model contains 1000 labels, part of them are omitted.

``` javascript
"output_postproc": [
    "layer_name": "prob", // (optional)
    "converter": "label",
    "method": "max",
    "labels": [
        "tench, Tinca tinca",
        "goldfish, Carassius auratus",
        "great white shark, white shark, man-eater, man-eating shark, Carcharodon carcharias",
        "tiger shark, Galeocerdo cuvieri",
        "hammerhead, hammerhead shark",
        ...,
        "earthstar",
        "hen-of-the-woods, hen of the woods, Polyporus frondosus, Grifola frondosa",
        "bolete",
        "ear, spike, capitulum",
        "toilet tissue, toilet paper, bathroom tissue"
    ]
]
```

#### Result

``` javascript
// squeezenet1.1.json
{
    "json_schema_version": "2.2.0",
    "input_preproc": [
        "format": "image",
        "layer_name": "data",
        "params": {
            "resize": "aspect-ratio"
        }
    ],
    "output_postproc": [
        "converter": "label",
        "method": "max",
        "labels": [
            "tench, Tinca tinca",
            "goldfish, Carassius auratus",
            "great white shark, white shark, man-eater, man-eating shark, Carcharodon carcharias",
            "tiger shark, Galeocerdo cuvieri",
            "hammerhead, hammerhead shark",
            ...,
            "earthstar",
            "hen-of-the-woods, hen of the woods, Polyporus frondosus, Grifola frondosa",
            "bolete",
            "ear, spike, capitulum",
            "toilet tissue, toilet paper, bathroom tissue"
        ]
    ]
}
```

### Build model-proc for detection model with advance post-processing

In this section, we will learn how to build a model-proc file for a
model
[YOLO v4 Tiny](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/yolo-v4-tiny-tf).
Let's start with an empty template:

``` javascript
// squeezenet1.1.json
{
    "json_schema_version": "2.2.0",
    "input_preproc": [],
    "output_postproc": []
}
```

#### Define "input_preproc"

The selected model has one input layer and it does not require a special
pre-processing algorithm -resize without aspect-ratio preservation is
enough. Therefore, we can leave this field empty: *"input_preproc": []*.
However, you are free to experiment and configure pre-processing
as you wish.

#### Define "output_postproc"

To begin with, we will determine which layers are the output ones.
Let's turn to the description of [Output of converted
model](https://github.com/openvinotoolkit/open_model_zoo/blob/master/models/public/yolo-v4-tiny-tf/README.md#converted-model-1).

1. The array of detection summary info, name -*conv2d_20/BiasAdd/Add*,
   shape - *1, 26, 26, 255*. The anchor values for each bbox on cell
   are *23,27, 37,58, 81,82*.
2. The array of detection summary info, name - *conv2d_17/BiasAdd/Add*,
   shape -*1, 13, 13, 255*. The anchor values bbox on cell are *81,82,
   135,169, 344,319*.

Thus:
*"layer_names": ["conv2d_20/BiasAdd/Add", "conv2d_17/BiasAdd/Add"]*,
*"anchors": [23.0, 27.0, 37.0, 58.0, 81.0, 82.0, 135.0, 169.0, 344.0, 319.0]*,
*"masks": [2, 3, 4, 0, 1, 2]*, *"bbox_number_on_cell": 3*,
*"cells_number": 13*.

The output of the model can be converted using *yolo_v3* converter since
it has a suitable structure.

Model was trained on COCO dataset with 80 classes: *"classes": 80*,
*"labels": ["person", "bicycle", "car", "motorbike", ..., "hair drier", "toothbrush"]*.

The parameters listed above are hyperparameters set when defining the
network architecture. It is known that YOLO models are anchor-based
models. This means that the network determines the classification of
objects in predetermined areas (bboxes) and adjusts the coordinates of
these areas. Roughly speaking, the whole picture is divided into regions
as follows: a grid of a certain size is imposed on the image
(*cells_number* depends on the size of the input layer and usually is
equal to *input_layer_size // 32*); then a certain number of bboxes of
different proportions (*bbox_number_on_cell*) are placed in each cell,
and the center of these bboxes coincides with the center of the cell;
then for each bbox (their number are *cells_number \* cells_number \*
bbox_number_on_cell*) the values *x, y, w, h, bbox_confidence* and
*class_1\_confidence, .., class_N\_confidence*, where *N = classes* are
predicted. Thus, the size of the **one** output layer should be equal to
*cells_number \* cells_number \* bbox_number_on_cell \* (5 + classes)*.
Note that the *anchors* values are compiled as
*\[x_coordinate_bbox_size_multiplier_1,
y_coordinate_bbox_size_multiplier_1, ..,
x_coordinate_bbox_size_multiplier_N,
y_coordinate_bbox_size_multiplier_N\]*, where *N = bbox_number_on_cell*.

> **NOTE:** In the case of multiple output layers, the grid size changes to
> accommodate smaller or larger objects. In this case, *cells_number* is
> specified for the layer with the smallest grid size. The grid sizes are
> sequentially doubled for each output layer: ([13, 13], [26, 26],
> [52, 52] ...) - other cases are not supported. In case this upsets
> you, please open an issue.

*masks* is about which set of anchors belongs to which output layer, in
the case of processing results from multiple layers. For example:
*number_of_outputs = 2, anchors: [x_1, y_1, x_2, y_2], masks: [0,
1]* - then for the first output layer *anchors: [x_1, y_1]* and for
the second *anchors: [x_2, y_2]*. Thus *bbox_number_on_cell = 1* will
be applied for each output.

Resume:

- *classes* - number of detection object classes (*optional if you set
  "labels" correctly*). You can get it from description of a model;
- *anchors* - one-dimensional array of anchors. See description of a
  model to get this parameter;
- *masks* - one-dimensional array contains subsets of anchors which
  correspond to output layers. Usually provided with documentation or
  architecture config as two-dimensional array, but you can pick up
  values by yourself;
- *cells_number* & *bbox_number_on_cell* - you can get them from
  model's architecture config or from information about dimensional
  of output layers. If you can not get it, you can solve the system of
  equations:

``` python
cells_number * cells_number * bbox_number_on_cell * (5 + classes) = min(len(output_blob_1), .., len(output_blob_N));
bbox_number_on_cell = len(anchors) / (N * 2);

where N is number of output layers.
```

Let's move on.
[Model's output description](https://github.com/openvinotoolkit/open_model_zoo/tree/master/models/public/yolo-v4-tiny-tf#converted-model-1)
says that it is necessary to apply the **sigmoid** functions to the
output values. Also we replaced the sigmoid call with softmax to
distribute the confidence values of the classes. This can be configured
with *"output_sigmoid_activation": true* and *"do_cls_softmax":
true* fields.

Next, to run the NMS algorithm, you need to set the parameter
*"iou_threshold": 0.4*, you can experiment with it to get a better
result in your task.

Thus, we have defined all the fields necessary for the *yolo_v3*
converter.

#### The Result

``` javascript
// yolo-v4-tiny-tf.json
{
    "json_schema_version": "2.2.0",
    "input_preproc": [],
    "output_postproc": [
        {
            "layer_names": ["conv2d_20/BiasAdd/Add", "conv2d_17/BiasAdd/Add"], // optional
            "converter": "yolo_v3",
            "anchors": [23.0, 27.0, 37.0, 58.0, 81.0, 82.0, 135.0, 169.0, 344.0, 319.0],
            "masks": [2, 3, 4, 0, 1, 2],
            "bbox_number_on_cell": 3,
            "cells_number": 13,
            "do_cls_softmax": true,
            "output_sigmoid_activation": true,
            "iou_threshold": 0.4,
            "classes": 80,
            "labels": [
                "person", "bicycle", "car",
                "motorbike", "aeroplane", "bus",
                ...,
                "teddy bear", "hair drier", "toothbrush"
            ]
        }
    ]
}
```
