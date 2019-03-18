# Shell samples
The shell samples demonstrate pipeline contruction and execution from command line through gst-launch utility.
See links
[Data flow](https://github.com/opencv/gst-video-analytics/wiki/Data-flow),
[Metadata](https://github.com/opencv/gst-video-analytics/wiki/Metadata),
[Properties](https://github.com/opencv/gst-video-analytics/wiki/Elements)
for more details about plugins functionality and parameters. 

The optional environment variable
```
VIDEO_EXAMPLES_DIR
```
used to specify directory with video files. The default location is 'video-examples' next to gstreamer-plugins repo.

Each shell script has default video file from 'video-examples' to run on. You can change the input video file in commandline parameter, for example:

```sh
./face_detection_and_classification.sh ~/gstreamer-plugins/video-examples/cool_video.mp4
```

The optional environment variable
```
GST_PLUGIN_PATH
```
used to specify directory with GVA plugins. Default location is gstreamer-plugins/build/intel64/Release/lib works well if follow build instructions or execute scripts/build.sh.

## Sample scripts descriptions:

### __face_detection_and_classification.sh__ <br>
Demonstrates face detection and face classification on two models, followed by metadata conversion into text form
  * __detection model:__ face-detection-adas-0001
  * __1st classification model:__ age-gender-recognition-retail-0013
  * __2nd classifiaction model:__ emotions-recognition-retail-0003
  * __converters used__:
    * converter=attributes method=max model=gender
    * converter=tensor2text model=age layer_name=age
    * converter=attributes method=max model=emotion

### __security_barrier_camera.sh__ <br>
Demonstrates vehicle and license-plate detection, and two classification models separately for vehicle and license-plate objects
  * __detection model__: vehicle-license-plate-detection-barrier-0106
  * __1st classification model:__ vehicle-attributes-recognition-barrier-0039
  * __2nd classification model:__ license-plate-recognition-barrier-0001
  * __converters used:__
    * converter=attributes method=max layer_name=color
    * converter=attributes method=max layer_name=type
    * converter=attributes method=index model=LPRNet

### __vehicle_and_pedestrian_classification.sh__ <br>
Demonstrates vehicle/bike/pedestrian detection, and two classification models separately for vehicle and pedestians
  * __detection model__: person-vehicle-bike-detection-crossroad-007
  * __1st classification model:__ person-attributes-recognition-crossroad-0200
  * __2nd classification model:__ vehicle-attributes-recognition-barrier-0039
  * __converters used:__
    * converter=attributes method=compound
    * converter=attributes method=max layer_name=color
    * converter=attributes method=max layer_name=type

### __vehicle_and_pedestrian_classification_videomemory.sh__ <br>
Same as previous but passing video memory through the whole pipeline

### __console_measure_fps.sh__ <br>
Measures and prints FPS, requires platforms with Intel GPU
  * __model:__ vehicle-license-plate-detection-barrier-0106

### __vehicle_detection_2sources_cpu.sh__ <br>
Demonstrates object detection on two source simultaniously with batching optimization across all source via 'inference-id' option
  * __model:__ vehicle-license-plate-detection-barrier-0106
  * __gvadetect parameters:__ inference-id=inf0 device=CPU

### __vehicle_detection_2sources_gpu.sh__ <br>
Same as previous but offloading inference from CPU to GPU

GVA plugins options and values:
* inference-id - the name of inference engine instance. Can be used to refer to the inference results originator element or to share IE instance between gvainference elements
* model - the name of IE model. Can be used to refer to the inference results originator model.
* layer_name - the name of IE model's output layer. Can be used to refer to the inference results output layer in multilayer output case.
* labels-file - the name of file with labels to be used in postprocessing stage
* converter - the name of __gvametaconvert__ data interpreter, one of the following:
  * attributes
  * tensor2text
  * json
  * as-detections
  * dump-detection
  * dump-tensors
* method - the method of _attributes_ (see above) data interpreter, one of the following:
   * max
   * compound - depending on threshold, even or odd label (from labels-file) will be selected
   * index
