Custom Processing
=================

The DL model inference in general works with tensor data on input and
output. The tensor is abstract N-dimension array, in GStreamer inference
plugins it stored under frame :doc:`Metadata <metadata>` using flexible
key-value container
`GstStructure <https://gstreamer.freedesktop.org/documentation/gstreamer/gststructure.html>`__.
The C++ class **GVA::Tensor** with `header-only implementation <https://github.com/dlstreamer/dlstreamer/blob/master/gst-libs/gst/videoanalytics/tensor.h#L38>`__
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
5. Modify source code of gvadetect/gvaclassify elements.

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
`draw_face_attributes <https://github.com/dlstreamer/dlstreamer/blob/master/samples/cpp/draw_face_attributes/main.cpp>`__

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
`sample <https://github.com/dlstreamer/dlstreamer/tree/master/samples/>`__

4. Insert new GStreamer element implemented on C/C++
----------------------------------------------------

Please refer to GStreamer documentation and samples how to implement new
GStreamer element and register GStreamer plugin.

If frame processing function implemented in C++, it can utilize
`GVA::Tensor <https://github.com/dlstreamer/dlstreamer/blob/master/gst-libs/gst/videoanalytics/tensor.h#L38>`__
helper class

5. Modify source code of post-processors for gvadetect/gvaclassify elements
---------------------------------------------------------------------------

You can add new or modify any suitable existing
`post-processor <https://github.com/dlstreamer/dlstreamer/blob/master/gst/inference_elements/common/post_processor/blob_to_meta_converter.cpp>`__
for gvadetect/gvaclassify elements.
