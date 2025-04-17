Model Info Section
==================

The OpenVINO™ Intermediate Representation (IR) includes an XML file with
description of network topology as well as conversion and runtime metadata.

If “model_proc" file is not present, Intel® Deep Learning Streamer parses
“model_info” section located at the end of the XML model file.
An example section looks as in the code snippet below:

..  code:: xml

	<rt_info>
		...
		<model_info>
			<iou_threshold value="0.7" />
			<labels value="person bicycle ... " />
			<model_type value="yolo_v8" />
			<pad_value value="114" />
			<resize_type value="fit_to_window_letterbox" />
			<reverse_input_channels value="True" />
			<scale_values value="255" />
		</model_info>
	</rt_info>

Intel® Deep Learning Streamer supports the following fields in the model info section:
 
.. list-table::
   :header-rows: 1
   
   * - Field
     - Type
     - Possible values or example
     - Description
     - Corresponding 'model-proc' key
   * - model_type
     - string
     - | label
       | detection_output
       | yolo_v8
     - The converter to parse output tensors and map to GStreamer meta data.
     - converter
   * - confidence_threshold
     - float
     - [0.0, 1.0]
     - The confidence level to report inference results, typically depends on training accuracy.
     - threshold (command line param)
   * - iou_threshold
     - float
     - [ 0.0, 1.0 ]
     - Threshold for non-maximum suppression (NMS) intersection over union (IOU) filtering.
     - iou_threshold
   * - multilabel
     - boolean
     - | True
       | False
     - Classification model predicts a set of labels per input image.
     - method=multi     
   * - output_raw_scores
     - boolean
     - | True
       | False
     - Classification model outputs all non-normalized scores for all detected labels.
     - method=softmax
   * - labels
     - string list
     - person bicycle ...
     - List of labels for predicted object classes.
     - labels
   * - resize_type
     - string
     - | crop
       | standard
       | fit_to_window
       | fit_to_window_letterbox
     - Resize method to map input video images to model input tensor.
     - resize
   * - reverse_input_channels
     - boolean
     - | True
       | False
     - Convert input video image to RGB format (model trained with RGB images)
     - color_space="RGB"
   * - scale
     - float
     - 255.0
     - Divide input image values by 'scale' before mapping to model input tensor
       (typically used when model was trained with input data normalized in <0,1> range).
     - range: [0.0, 1.0]

Please also refer to `OpenVINO™ Model API <https://github.com/openvinotoolkit/model_api/blob/master/docs/source/guides/model-configuration.md>`__
for more information on the “model_info” section. 