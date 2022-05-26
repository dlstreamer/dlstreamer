Object Tracking
===============

Object tracking types
---------------------

Object tracking element :doc:`gvatrack <../elements/gvatrack>` typically inserted into
video analytics pipeline right after object detection element
:doc:`gvadetect <../elements/gvadetect>` and can work in the following modes as
specified by property *tracking-type*:

.. list-table::
   :header-rows: 1
   :widths: 15,15,15,15,40
   
   * - tracking-type
     - Max detection interval (inference -interval property in gvadetect)
     - Algorithm uses detected coordinates and trajectory extrapolation
     - Algorithm uses image data
     - Notes
   * - short‑term‑imageless
     - <= 5
     - Yes
     - No
     - Assigns unique id to objects and generates object position on frames object detection was skipped, by extrapolati ng object trajectory on previous frames. Fast algorithm without access to image data.
   * - zero‑ term
     - 1 (every frame)
     - Yes
     - Yes
     - Assigns unique id to objects, requires object detection run every frame. Takes into account object trajectory as well as color histogram of object image data.
   * - zero‑term‑imageless
     - 1 (every frame)
     - Yes
     - No
     - Assigns unique id to objects, requires object detection run every frame. Fastest algorithm as based on comparing object coordinates on current frame with objects trajectory on previous frames.

Additional configuration
------------------------

Additional configuration parameters for object tracker can be passed via
``config`` property of :doc:`gvatrack <../elements/gvatrack>`. The ``config`` property
accepts a comma separated list of ``KEY=VALUE`` parameters. The
supported parameters are described below:

Tracking per class
^^^^^^^^^^^^^^^^^^

Configurable via ``tracking_per_class`` parameter. It specifies whether
the class label is considered for updating the ``object_id`` of an
object or not. When set to ``true``, a new tracking ID will be assigned
to an object when the class label changes. When set to ``false``, the
tracking ID will be retained based on the position of the bounding box,
even if the class label of the object changes due to model inaccuracy.
The default value is ``true``.

Example:

::

   ... gvatrack config=tracking_per_class=false ...

Maximum number of objects
^^^^^^^^^^^^^^^^^^^^^^^^^

Configurable via ``max_num_objects`` parameter. It specifies the maximum
number of the objects that the object tracker will track. On devices
with less computing power, tracking smaller number of objects can reduce
compute and increase throughput.

Example:

::

   ... gvatrack config=max_num_objects=20 ...

Sample
------

Please refer to sample
`vehicle_pedestrian_tracking <https://github.com/dlstreamer/dlstreamer/tree/master/samples/gst_launch/vehicle_pedestrian_tracking>`__
for pipeline example with ``gvadetect``, ``gvatrack``, ``gvaclassify``
elements

How to read object unique id
----------------------------

The following code example iterates all objects detected or tracked on
current frame and prints object unique id and bounding box coordinates

::

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

Performance considerations
--------------------------

Object tracking can help improve performance of both object detection
(gvadetect) and object classification (gvaclassify) elements

* Object detection: *short-term-imageless* tracking types allow
  to reduce object detection frequency by setting property ``inference-interval`` in gvadetect element.
* Object classification: if object was classified by 'gvaclassify' on frame N, we can skip
  classification of the same object for several next frames N+1,N+2,… and reuse last classification result from frame N.
  Reclassification interval is controlled by property ``reclassify-interval`` in 'gvaclassify' element.

For example the following pipeline

::

     ... ! \
     decodebin ! \
     gvadetect model=$DETECTION_MODEL inference-interval=10 ! \
     gvatrack tracking-type=short-term-imageless ! \
     gvaclassify model=$AGE_GENDER_MODEL reclassify-interval=30 ! \
     gvaclassify model=$EMOTION_MODEL reclassify-interval=15 ! \
     gvaclassify model=$LANDMARKS_MODEL ! \
     ...

detects faces every 10th frame and tracks faces position next 9 frames,
age and gender classification updated once a second, emotion
classification updated twice a second, landmark points updated every
frame.
