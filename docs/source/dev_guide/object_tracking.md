# Object Tracking

## Object tracking types

[gvatrack](../elements/gvatrack.md) is an object tracking element, typically inserted into a
video analytics pipeline right after the object
detection element [gvadetect](../elements/gvadetect.md)
and can work in the following modes as specified by the *tracking-type* property:

| tracking-type        | Max detection interval (inference-interval property in gvadetect) | Algorithm uses detected coordinates and trajectory extrapolation | Algorithm uses image data | Notes                                                                                                                                                                                                                     |
|----------------------|-------------------------------------------------------------------|------------------------------------------------------------------|---------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| short-term-imageless | &lt;= 5                                                           | Yes                                                              | No                        | Assigns a unique id to objects and generates object position for frames on which object detection was skipped.<br>Fast algorithm that extrapolates object trajectory from previous frame(s) without access to image data. |
| zero-term            | 1 (every frame)                                                   | Yes                                                              | Yes                       | Assigns a unique id to objects, and requires object detection run on every frame.<br>Takes into account object trajectory as well as a color histogram of object image data.                                              |
| zero-term-imageless  | 1 (every frame)                                                   | Yes                                                              | No                        | Assigns a unique id to objects, and requires object detection run on every frame.<br>Fastest algorithm as based on comparing object coordinates on current frame with objects trajectory on previous frames.              |

## Additional configuration

Additional configuration parameters for object tracker can be passed via the
`config` property of [gvatrack](../elements/gvatrack.md).
The `config` property accepts a comma separated list of
`KEY=VALUE` parameters. The supported parameters are described below:

### Tracking per class

Configurable via `tracking_per_class` parameter. It specifies whether
the class label is considered for updating the `object_id` of an object
or not. When set to `true`, a new tracking ID will be assigned to an
object when the class label changes. When set to `false`, the tracking
ID will be retained, based on the position of the bounding box, even if
the class label of the object changes due to model inaccuracy. The
default value is `true`.

Example:

```bash
... gvatrack config=tracking_per_class=false ...
```

### Maximum number of objects

Configurable via `max_num_objects` parameter. It specifies the maximum
number of objects that the object tracker will track. On devices with
less computing power, tracking a smaller number of objects can reduce
compute and increase throughput.

Example:

```bash
... gvatrack config=max_num_objects=20 ...
```

## Sample

Refer to the
[vehicle_pedestrian_tracking](https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/vehicle_pedestrian_tracking) sample
for a pipeline with `gvadetect`, `gvatrack`, and `gvaclassify` elements.

## How to read object unique id

The following code example iterates all objects detected or tracked on
the current frame and prints object unique id and bounding box
coordinates.

```cpp
#include "video_frame.h"

void PrintObjects(GstBuffer *buffer) {
    GVA::VideoFrame video_frame(buffer);
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    for (GVA::RegionOfInterest &roi : regions) { // iterate objects
        int object_id = roi.object_id(); // get unique object id
        auto bbox = roi.rect(); // get bounding box information
        std::cout << "Object id=" << object_id << ", bounding box: " << bbox.x << "," << bbox.y << "," << bbox.w << "," << bbox.h << "," << std::endl;
    }
}
```

## Performance considerations

Object tracking can help improve performance of both object detection
`(`gvadetect`) and object classification (`gvaclassify`) elements

- Object detection: *short-term-imageless* tracking types enable
  reducing object detection frequency by setting the
  `inference-interval` property in the `gvadetect` element.
- Object classification: if an object was classified by `gvaclassify`
  on frame N, you can skip classification of the same object for
  several next frames N+1,N+2,... and reuse last classification result
  from frame N. Reclassification interval is controlled by the
  `reclassify-interval` property in the `gvaclassify` element.

See the sample pipeline below:

```bash
gst-launch-1.0 \
... ! \
decodebin3 ! \
gvadetect model=$DETECTION_MODEL inference-interval=10 ! \
gvatrack tracking-type=short-term-imageless ! \
gvaclassify model=$AGE_GENDER_MODEL reclassify-interval=30 ! \
gvaclassify model=$EMOTION_MODEL reclassify-interval=15 ! \
gvaclassify model=$LANDMARKS_MODEL ! \
...
```

It detects faces every 10th frame and tracks faces position for the next 9 frames.
The age and gender classification is updated every second, the emotion
classification is updated twice a second, and the landmark points are updated every
frame.
