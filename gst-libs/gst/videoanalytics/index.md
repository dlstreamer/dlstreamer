# GStreamer Video Analytics plugin documentation
This documentation describes Gstreamer Video Analytics interfaces to access and control GStreamer metadata, which contains models tensors as a result of inference with GStreamer elements, such as **gvainference/gvadetect/gvaclassify**.  

Inference plugins utilize [standard GStreamer metadata **GstVideoRegionOfInterestMeta**](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) for object detection, classification and recognition use cases (elements **gvadetect, gvaclassify, gvaidentify**), and define two custom metadata types
* _GstGVATensorMeta for output of **gvainference** element performing generic inference on any model with image-compatible input layer and any format of output layer(s)
* _GstGVAJSONMeta for output of **gvametaconvert** element performing [**GstVideoRegionOfInterestMeta**](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) conversion into JSON format

The **gvadetect** element supports only object detection models and checks model output layer to have known format convertable into bounding boxes list. The gvadetect element creates and attaches to output GstBuffer as many instances of [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) as objects detected on the frame. The object bounding-box position and object label stored directly in [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) fields 'x','y','w','h','roi_type', while additional detection information such as confidence (in range [0,1]), model name and output layer name stored as GstStructure object and added into 'GList *params' list of the same [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275).

The **gvaclassify** element typically inserted into pipeline after gvadetect and executes inference on all objects detected by gvadetect (i.e as many times as [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) attached to input buffer) with input on crop area specified by [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275). The inference output converted into as many GstStructure objects as number of output layers in the model and added into 'GList *params' list of the [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275). Each GstStructure contains full inference results such as tensor data and dimensions, model and layer names, and label in string format (if post-processing rules specified).

The **gvainference** element generates and attaches to the frame custom metadata _GstGVATensorMeta (as many instances as output layers in the model) containing tensor raw data and additional information such as tensor dimensions, data precision, etc.

Taking the following pipeline as example:
```sh
MODEL1=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013
MODEL3=emotions-recognition-retail-0003

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
    filesrc location=${INPUT} ! decodebin ! video/x-raw ! videoconvert ! \
    gvadetect   model=$(MODEL_PATH $MODEL1) ! queue ! \
    gvaclassify model=$(MODEL_PATH $MODEL2) model-proc=$(PROC_PATH $MODEL2) ! queue ! \
    gvaclassify model=$(MODEL_PATH $MODEL3) model-proc=$(PROC_PATH $MODEL3) ! queue ! \
    gvawatermark ! videoconvert ! fpsdisplaysink video-sink=ximagesink sync=false
```
The gvadetect detected three faces, it will attach three metadata objects each containing one GstStructure with detection results, the next gvaclassify will add two more GstStructure (model contains two output layers, age and gender) into each meta, and another gvaclassify will add one more GstStructure (emotion), resulting in three metadata objects each containing four GstStructure in 'GList *params' field: detection, age, gender, emotions.

"C"/C++ application can iterate objects and inference results using GStreamer API similar to code snapshot below

```
#include <gst/video/video.h>
void print_meta(GstBuffer *buffer) {
    gpointer state = NULL;
    GstMeta *meta = NULL;
    while ((meta = gst_buffer_iterate_meta(buffer, &state)) != NULL) {
        if (meta->info->api != GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE)
            continue;
        GstVideoRegionOfInterestMeta *roi_meta = (GstVideoRegionOfInterestMeta*)meta;
        printf("Object bounding box %d,%d,%d,%d\n", roi_meta->x, roi_meta->y, roi_meta->w, roi_meta->h);
        for (GList *l = roi_meta->params; l; l = g_list_next(l)) {
            GstStructure *structure = (GstStructure *) l->data;
            printf("  Attribute %s\n", gst_structure_get_name(structure));
            if (gst_structure_has_field(structure, "label")) {
                printf("    label=%s\n", gst_structure_get_string(structure, "label"));
            }
            if (gst_structure_has_field(structure, "confidence")) {
                double confidence;
                gst_structure_get_double(structure, "confidence", &confidence);
                printf("    confidence=%.2f\n", confidence);
            }
        }
    }
}
```

For C++ application the the header-only C++ utility gva_roi_meta.h helps to simplify the code above by C++ classes with get/set functions and C++ iterators for [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275). GVA::RegionOfInterest and GVA::RegionOfInterestList are classes to refer and interate through metadata accordingly.  
```
#include "gva_roi_meta.h"
void PrintMeta(GstBuffer *buffer) {
    GVA::RegionOfInterestList roi_list(buffer);
    for (GVA::RegionOfInterest &roi : roi_list) {
        GstVideoRegionOfInterestMeta *meta = roi.meta();
        std::cout << "Object bounding box " << meta->x << "," << meta->y << "," << meta->w << "," << meta->h << "," << std::endl;
        for (GVA::Tensor &tensor : roi) {
            std::cout << "  Attribute " << tensor.name() << std::endl;
            std::cout << "    label=" << tensor.label() << std::endl;
            std::cout << "    confidence=" << tensor.confidence() << std::endl;
        }
    }
}
```

The following table summarizes the input and output of various elements

| GStreamer element | Description | INPUT | OUTPUT |
| --- | --- | --- | --- |
| gvainference | Generic inference| GstBuffer | INPUT + _GstGVATensorMeta |
| gvadetect| Object detection| GstBuffer| INPUT + [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275)|
| gvaclassify| Object classification| GstBuffer + [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) | INPUT + extended [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) |
| gvametaconvert| Metadata conversion | GstBuffer + [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) | INPUT + _GstGVAJSONMeta|
| gvawatermark| Overlay| GstBuffer + [GstVideoRegionOfInterestMeta](https://github.com/GStreamer/gst-plugins-base/blob/master/gst-libs/gst/video/gstvideometa.h#L275) | - |
