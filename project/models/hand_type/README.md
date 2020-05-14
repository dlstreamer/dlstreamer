# hand_type

## Usage:

1. Run gst:
    * To display:
```
gst-launch-1.0 filesrc location=<video> ! decodebin ! videoconvert ! video/x-raw,format=BGR ! queue ! gvaskeleton model_path=~/Downloads/human-pose-estimation-0001.xml device=CPU render=false hands-detect=true ! gvaclassify model=<models folder>/hand_type/hand_type.xml model-proc=<models folder>/hand_classify/hand_classify.json ! gvawatermark ! videoconvert ! fpsdisplaysink sink=xvimagesink sync=false
```
