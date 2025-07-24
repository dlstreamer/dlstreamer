Custom Processing
=================

The DL model inference in general works with tensor data on input and
output. The tensor is abstract N-dimension array, in GStreamer inference
plugins it stored under frame :doc:`Metadata <metadata>` using flexible
key-value container
`GstStructure <https://gstreamer.freedesktop.org/documentation/gstreamer/gststructure.html>`__.
The C++ class **GVA::Tensor** with `header-only implementation <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/include/dlstreamer/gst/videoanalytics/tensor.h#L38>`__
helps C++ applications access the tensor data.

The DL model inference integration into real application typically
involves model specific pre-processing and post-processing logic.
GStreamer inference plugins support pre-/post-processing for many
popular model topologies and use-cases, configurable via JSON file
format as described in :doc:`model preparation <model_preparation>` page.

If specific model pre-/post-processing not supported, there are several
options to customize GStreamer pipeline or offload processing logic to
application:

1. Consume tensor data and parse/convert/process it on application side;
2. Set C/Python callback in the middle of GStreamer pipeline;
3. Insert gvapython element and provide Python callback function;
4. Insert new GStreamer element implemented on C/C++;
5. Modify source code of gvadetect/gvaclassify elements;
6. Create custom post-processing library.

Next sections talk about these options in more details.

1. Consume tensor data and parse/convert it on application side
---------------------------------------------------------------

This option works in most use cases except

- Converted/parsed data required by downstream element (example, object detection->classification chain)
- Pipeline constructed and executed by gst-launch command-line utility, not by C/C++/Python application

The C/C++ application can either

- Set pad probe callback on one of element at the end of pipeline (after all metadata attached to frame). The callback mechanism documented `by GStreamer framework <https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html#data-probes>`__
- Insert **appsink** element at the end of pipeline and utilize **appsink** functions and signals for GstBuffer and metadata consumption

The pad probe callback demonstrated in C++ sample
`draw_face_attributes <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/cpp/draw_face_attributes/main.cpp>`__

2. Set C/Python callback in the middle of GStreamer pipeline
------------------------------------------------------------

Similar to 1, pad probe callback could be set in the middle of pipeline.

Note that GstBuffer on source pad (output pad) of all inference elements
guaranteed to be writable (gst_buffer_is_writable returns true) so
application specific C++/Python callback or custom element can
attach/modify GstVideoRegionOfInterestMeta or other metadata to the
GstBuffer.

This enables pipelines **gvainference->gvaclassify** where gvainference
runs object detection model with custom output layer format. C/Python
callback inserted by the app between gvainference and gvaclassify can
parse tensor data in GvaTensorMeta into list of objects (bounding box
with attributes), and attach new GstVideoRegionOfInterestMeta to video
frame for further object classification by downstream element
gvaclassify.

3. Insert gvapython element and provide Python callback function
----------------------------------------------------------------

The advantage of this option is applicability for any application type
including gst-launch utility.

The :doc:`gvapython element <../elements/gvapython>` takes reference to user provided
Python script with function to be called on every frame processing.

The callback function can attach/modify metadata as demonstrated in
`sample <https://github.com/dlstreamer/dlstreamer/tree/master/samples/gstreamer/gst_launch/gvapython/face_detection_and_classification>`__

4. Insert new GStreamer element implemented on C/C++
----------------------------------------------------

Please refer to GStreamer documentation and samples how to implement new
GStreamer element and register GStreamer plugin.

If frame processing function implemented in C++, it can utilize
`GVA::Tensor <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/include/dlstreamer/gst/videoanalytics/tensor.h#L38>`__
helper class

5. Modify source code of post-processors for gvadetect/gvaclassify elements
---------------------------------------------------------------------------

You can add new or modify any suitable existing
`post-processor <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/src/monolithic/gst/inference_elements/common/post_processor/blob_to_meta_converter.cpp>`__
for gvadetect/gvaclassify elements.

6. Create custom post-processing library
----------------------------------------

For advanced custom post-processing scenarios, you can create a separate dynamic library
that implements the post-processing logic without modifying the DL Streamer source code.
This approach provides flexibility and modularity while maintaining clean separation
between the core framework and custom processing logic.

A practical example implementations demonstrated in
`sample <https://github.com/open-edge-platform/edge-ai-libraries/tree/main/libraries/dl-streamer/samples/gstreamer/gst_launch/custom_postproc>`__

**Important Requirements**

Your custom library must use the **GStreamer Analytics Library** which provides
standardized metadata structures for AI/ML results. The library implements structures
such as ``GstTensorMeta``, ``GstAnalyticsRelationMeta``, ``GstAnalyticsODMtd``,
``GstAnalyticsClsMtd``, and others.

For more information about the Analytics metadata library, please refer to:
`GStreamer Analytics Documentation <https://gstreamer.freedesktop.org/documentation/analytics/index.html?gi-language=c>`__

**Current Support Limitations**

At this time, support is available only for **detection** and **classification** tasks:

- **Object Detection** (``GstAnalyticsODMtd``) - works only with ``gvadetect`` element
- **Classification** (``GstAnalyticsClsMtd``) - works with both ``gvadetect`` and ``gvaclassify`` elements

**Implementation Requirements**

Your custom library must export a ``Convert`` function with the following signature:

.. code-block:: c

   void Convert(GstTensorMeta *outputTensors,
                const GstStructure *network,
                const GstStructure *params,
                GstAnalyticsRelationMeta *relationMeta);

Where:

- ``outputTensors`` - contains output tensor data from the model inference
- ``network`` - model metadata including labels, input dimensions
- ``params`` - processing parameters like confidence thresholds
- ``relationMeta`` - output structure for attaching results

**Important Notes:**

- Each model output layer has a separate ``GstTensor`` contained within one ``GstTensorMeta``. Tensors from individual layers can be identified by their ``GstTensor`` IDs.
- Regardless of the ``batch-size`` setting in ``gvadetect`` or ``gvaclassify`` elements, the output tensors from the model are always passed to the ``Convert`` function in an **unbatched** format (i.e., with batch dimension equal to 1).

**Usage in GStreamer Pipeline**

Use the ``custom-postproc-lib`` parameter directly in DLS elements
(``gvadetect`` or ``gvaclassify``):

.. code-block:: bash

   gst-launch-1.0 videotestsrc ! gvadetect \
     model=/path/to/model.xml \
     custom-postproc-lib=/path/to/your/libcustom_postproc.so ! \
     ...

**Example Implementation**

Here are examples of custom post-processing libraries for both use cases:

**Example 1: Object Detection**

.. code-block:: c

   #include <gst/gst.h>
   #include <gst/analytics/analytics.h>
   #include <stdexcept>
   #include <vector>

   extern "C" void Convert(GstTensorMeta *outputTensors,
                           const GstStructure *network,
                           const GstStructure *params,
                           GstAnalyticsRelationMeta *relationMeta) {

       // Get output tensor(s)
       const GstTensor *tensor = gst_tensor_meta_get(outputTensors, 0);
       size_t dims_size;
       size_t *dims = gst_tensor_get_dims(gst_tensor_copy(tensor), &dims_size);

       // Get network metadata
       size_t input_width = 0, input_height = 0;
       gst_structure_get_uint64(network, "image_width", &input_width);
       gst_structure_get_uint64(network, "image_height", &input_height);

       // Get processing parameters
       double confidence_threshold = 0.5;
       gst_structure_get_double(params, "confidence_threshold", &confidence_threshold);

       // Get class labels
       std::vector<std::string> labels;
       const GValue *labels_value = gst_structure_get_value(network, "labels");
       if (labels_value && G_VALUE_HOLDS(labels_value, GST_TYPE_ARRAY)) {
           int n_labels = gst_value_array_get_size(labels_value);
           for (int i = 0; i < n_labels; ++i) {
               const GValue *item = gst_value_array_get_value(labels_value, i);
               if (G_VALUE_HOLDS_STRING(item))
                   labels.push_back(g_value_get_string(item));
           }
       }

       // Map tensor data to access raw model output
       float *data = nullptr;
       GstMapInfo map;
       if (gst_buffer_map(tensor->data, &map, GST_MAP_READ)) {
           data = reinterpret_cast<float *>(map.data);
           gst_buffer_unmap(tensor->data, &map);
       } else {
           throw std::runtime_error("Failed to map tensor data.");
       }

       // Process model output according to your specific model format
       // Parse detection results: bounding boxes, confidence scores, class IDs
       // Apply confidence thresholding and NMS if needed
       // ...

       // For each detected object, add object detection metadata
       int x = 100, y = 50, w = 200, h = 150;  // Example coordinates
       float confidence = 0.85;                // Example confidence
       size_t class_id = 0;                   // Example class index

       GQuark label_quark = g_quark_from_string(labels[class_id].c_str());

       GstAnalyticsODMtd od_mtd;
       if (!gst_analytics_relation_meta_add_od_mtd(relationMeta, label_quark,
                                                  x, y, w, h, confidence, &od_mtd)) {
           throw std::runtime_error("Failed to add object detection metadata.");
       }
   }

**Example 2: Classification**

.. code-block:: c

   #include <gst/gst.h>
   #include <gst/analytics/analytics.h>
   #include <algorithm>
   #include <stdexcept>
   #include <vector>

   extern "C" void Convert(GstTensorMeta *outputTensors,
                           const GstStructure *network,
                           const GstStructure *params,
                           GstAnalyticsRelationMeta *relationMeta) {

       // Get classification output tensor
       const GstTensor *tensor = gst_tensor_meta_get(outputTensors, 0);
       size_t dims_size;
       size_t *dims = gst_tensor_get_dims(gst_tensor_copy(tensor), &dims_size);

       size_t num_classes = dims[dims_size - 1];

       // Get network metadata
       size_t input_width = 0, input_height = 0;
       gst_structure_get_uint64(network, "image_width", &input_width);
       gst_structure_get_uint64(network, "image_height", &input_height);

       // Specify confidence threshold
       double confidence_threshold = 0.5;

       // Get class labels
       std::vector<std::string> labels;
       const GValue *labels_value = gst_structure_get_value(network, "labels");
       if (labels_value && G_VALUE_HOLDS(labels_value, GST_TYPE_ARRAY)) {
           int n_labels = gst_value_array_get_size(labels_value);
           for (int i = 0; i < n_labels; ++i) {
               const GValue *item = gst_value_array_get_value(labels_value, i);
               if (G_VALUE_HOLDS_STRING(item))
                   labels.push_back(g_value_get_string(item));
           }
       }

       // Map tensor data to access raw model output
       float *data = nullptr;
       GstMapInfo map;
       if (gst_buffer_map(tensor->data, &map, GST_MAP_READ)) {
           data = reinterpret_cast<float *>(map.data);
           gst_buffer_unmap(tensor->data, &map);
       } else {
           throw std::runtime_error("Failed to map tensor data.");
       }

       // Process classification output according to your model format
       // Apply softmax, find top-k classes, or other post-processing
       // ...

       // Example: find class with highest score
       size_t best_class_id = 0;
       float best_confidence = 0.8;  // Example confidence score

       if (best_confidence > confidence_threshold && best_class_id < labels.size()) {
           std::string label = labels[best_class_id];
           GQuark label_quark = g_quark_from_string(label.c_str());

           // Add classification metadata
           GstAnalyticsClsMtd cls_mtd;
           if (!gst_analytics_relation_meta_add_one_cls_mtd(relationMeta, best_confidence,
                                                           label_quark, &cls_mtd)) {
               throw std::runtime_error("Failed to add classification metadata.");
           }
       }
   }

**Compilation**

Compile your library as a shared object with GStreamer Analytics support:

.. code-block:: bash

   g++ -shared -fPIC -o libcustom_postproc.so custom_postproc.cpp \
     `pkg-config --cflags --libs gstreamer-1.0 gstreamer-analytics-1.0` -ldl -Wl,--no-undefined
