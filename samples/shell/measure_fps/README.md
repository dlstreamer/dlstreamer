# Measure FPS Python sample
**Measure FPS Python sample** - the example, which, due to its flexibility, allows you to quickly create a detection (& classification) pipeline, launched on your chosen device. You can also specify the number of channels launched, the flag for using the graphic output of the processed video, and objects that should be classified.
To start with the default configuration, you need to run the sample and, without fail, specify the path to the video under investigation.
```shell
python3 measure_fps.py /path/to/your/video
```
Then the following pipeline will be launched:
```shell
gst-launch-1.0 \
--gst-plugin-path /usr/lib/gst-video-analytics/ filesrc location=/path/to/your/video ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=BGR ! gvadetect model=/path/to/model.xml model-proc=/path/to/model_proc.json device=CPU ! queue ! gvafpscounter ! fakesink
```
Consider a more advanced launch:
```shell
python3 measure_fps.py /path/to/your/video -g --detection_model=dtct_model \
    --classification_models="clsf_model_1(some_object_1),clsf_model_2(some_object_2)" \
    --num_channels=4 --device=GPU
```
When declaring a classification model, the objects to be classified are indicated in brackets.
Thus, **security_barrier_camera** sample is formed and launched in 4 channels on the GPU:
```shell
gst-launch-1.0 \
--gst-plugin-path /usr/lib/gst-video-analytics/ filesrc location=/path/to/your/video ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=BGR ! gvadetect model=/path/to/dtct_model.xml model-proc=/path/to/model_proc.json device=GPU ! queue ! gvaclassify model=/path/to/clsf_model_1.xml model-proc=/path/to/clsf_model_proc_1.json device=GPU object-class=some_object_1 ! queue ! gvaclassify model=/path/to/clsf_model_2.xml model-proc=/path/to/clsf_model_proc_2.json device=GPU object-class=some_object_2 ! queue ! gvafpscounter ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false \
--gst-plugin-path /usr/lib/gst-video-analytics/ filesrc location=/path/to/your/video ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=BGR ! gvadetect model=/path/to/dtct_model.xml model-proc=/path/to/model_proc.json device=GPU ! queue ! gvaclassify model=/path/to/clsf_model_1.xml model-proc=/path/to/clsf_model_proc_1.json device=GPU object-class=some_object_1 ! queue ! gvaclassify model=/path/to/clsf_model_2.xml model-proc=/path/to/clsf_model_proc_2.json device=GPU object-class=some_object_2 ! queue ! gvafpscounter ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false \
--gst-plugin-path /usr/lib/gst-video-analytics/ filesrc location=/path/to/your/video ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=BGR ! gvadetect model=/path/to/dtct_model.xml model-proc=/path/to/model_proc.json device=GPU ! queue ! gvaclassify model=/path/to/clsf_model_1.xml model-proc=/path/to/clsf_model_proc_1.json device=GPU object-class=some_object_1 ! queue ! gvaclassify model=/path/to/clsf_model_2.xml model-proc=/path/to/clsf_model_proc_2.json device=GPU object-class=some_object_2 ! queue ! gvafpscounter ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false \
--gst-plugin-path /usr/lib/gst-video-analytics/ filesrc location=/path/to/your/video ! decodebin ! videoconvert ! videoscale ! video/x-raw,format=BGR ! gvadetect model=/path/to/dtct_model.xml model-proc=/path/to/model_proc.json device=GPU ! queue ! gvaclassify model=/path/to/clsf_model_1.xml model-proc=/path/to/clsf_model_proc_1.json device=GPU object-class=some_object_1 ! queue ! gvaclassify model=/path/to/clsf_model_2.xml model-proc=/path/to/clsf_model_proc_2.json device=GPU object-class=some_object_2 ! queue ! gvafpscounter ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```
The result of the script execution is the output logs, the process termination code and the average FPS value.
