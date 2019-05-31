/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvadetect.h"
#include "inference_impl.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gva_roi_meta.h>
#include <opencv2/imgproc.hpp>

#include "config.h"

#define ELEMENT_LONG_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

enum {
    PROP_0,
    PROP_THRESHOLD,
};

#define DEFALUT_MIN_THRESHOLD 0.
#define DEFALUT_MAX_THRESHOLD 1.
#define DEFALUT_THRESHOLD 0.5

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define VA_SURFACE_CAPS

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")
#define INFERENCE_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS INFERENCE_CAPS
#define VIDEO_SRC_CAPS INFERENCE_CAPS

void ExtractBoundingBoxes(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                          std::vector<InferenceROI> frames, const std::map<std::string, GstStructure *> &model_proc,
                          const gchar *model_name, GvaBaseInference *gva_base_inference) {
    for (const auto &blob_desc : output_blobs) {
        InferenceBackend::OutputBlob::Ptr blob = blob_desc.second;
        if (blob == nullptr)
            throw std::runtime_error("Blob is empty. Cannot access to null object.");

        const std::string &layer_name = blob_desc.first;

        const float *detections = (const float *)blob->GetData();
        auto dims = blob->GetDims();
        auto layout = blob->GetLayout();

        GST_DEBUG("DIMS:\n");
        for (auto dim = dims.begin(); dim < dims.end(); dim++) {
            GST_DEBUG("\t%lu\n", *dim);
        }

        gint object_size = 0;
        gint max_proposal_count = 0;
        switch (layout) {
        case InferenceBackend::OutputBlob::Layout::NCHW:
            object_size = dims[3];
            max_proposal_count = dims[2];
            break;
        default:
            GST_ERROR("Unsupported output layout, boxes won't be extracted\n");
            continue;
        }
        if (object_size != 7) { // SSD DetectionOutput format
            GST_ERROR("Unsupported output dimensions, boxes won't be extracted\n");
            continue;
        }

        // Read labels and roi_scale from GstStructure config
        GValueArray *labels = nullptr;
        double roi_scale = 1.0;
        auto post_proc = model_proc.find(layer_name);
        if (post_proc != model_proc.end()) {
            gst_structure_get_array(post_proc->second, "labels", &labels);
            gst_structure_get_double(post_proc->second, "roi_scale", &roi_scale);
        }

        double threshold = ((GstGvaDetect *)gva_base_inference)->threshold;

        for (int i = 0; i < max_proposal_count; i++) {
            int image_id = (int)detections[i * object_size + 0];
            int label_id = (int)detections[i * object_size + 1];
            double confidence = detections[i * object_size + 2];
            double x_min = detections[i * object_size + 3];
            double y_min = detections[i * object_size + 4];
            double x_max = detections[i * object_size + 5];
            double y_max = detections[i * object_size + 6];
            if (image_id < 0 || (size_t)image_id >= frames.size()) {
                break;
            }
            if (confidence < threshold) {
                continue;
            }
            int width = frames[image_id].roi.w;
            int height = frames[image_id].roi.h;

            const gchar *label = NULL;
            if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
                label = g_value_get_string(labels->values + label_id);
            }
            if (roi_scale > 0 && roi_scale != 1) {
                double x_center = (x_max + x_min) * 0.5;
                double y_center = (y_max + y_min) * 0.5;
                double new_w = (x_max - x_min) * roi_scale;
                double new_h = (y_max - y_min) * roi_scale;
                x_min = x_center - new_w * 0.5;
                x_max = x_center + new_w * 0.5;
                y_min = y_center - new_h * 0.5;
                y_max = y_center + new_h * 0.5;
            }
            gint ix_min = (gint)(x_min * width + 0.5);
            gint iy_min = (gint)(y_min * height + 0.5);
            gint ix_max = (gint)(x_max * width + 0.5);
            gint iy_max = (gint)(y_max * height + 0.5);
            if (ix_min < 0)
                ix_min = 0;
            if (iy_min < 0)
                iy_min = 0;
            if (ix_max > width)
                ix_max = width;
            if (iy_max > height)
                iy_max = height;
            GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
                frames[image_id].buffer, label, ix_min, iy_min, ix_max - ix_min, iy_max - iy_min);

            GstStructure *s =
                gst_structure_new("detection", "confidence", G_TYPE_DOUBLE, confidence, "label_id", G_TYPE_INT,
                                  label_id, "x_min", G_TYPE_DOUBLE, x_min, "x_max", G_TYPE_DOUBLE, x_max, "y_min",
                                  G_TYPE_DOUBLE, y_min, "y_max", G_TYPE_DOUBLE, y_max, "model_name", G_TYPE_STRING,
                                  model_name, "layer_name", G_TYPE_STRING, layer_name.c_str(), NULL);
            gst_video_region_of_interest_meta_add_param(meta, s);
        }

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        if (labels)
            g_value_array_free(labels);
        G_GNUC_END_IGNORE_DEPRECATIONS
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Register element

extern "C" {

GST_DEBUG_CATEGORY_STATIC(gst_gva_detect_debug_category);
#define GST_CAT_DEFAULT gst_gva_detect_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaDetect, gst_gva_detect, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_detect_debug_category, "gvadetect", 0,
                                                "debug category for gvadetect element"));

void gst_gva_detect_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaDetect *gvadetect = (GstGvaDetect *)(object);

    GST_DEBUG_OBJECT(gvadetect, "set_property");

    switch (property_id) {
    case PROP_THRESHOLD:
        gvadetect->threshold = g_value_get_double(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_detect_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaDetect *gvadetect = (GstGvaDetect *)(object);

    GST_DEBUG_OBJECT(gvadetect, "get_property");

    switch (property_id) {
    case PROP_THRESHOLD:
        g_value_set_double(value, gvadetect->threshold);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_detect_class_init(GstGvaDetectClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_detect_set_property;
    gobject_class->get_property = gst_gva_detect_get_property;
    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_float("threshold", "Threshold", "Threshold for inference",
                                                       DEFALUT_MIN_THRESHOLD, DEFALUT_MAX_THRESHOLD, DEFALUT_THRESHOLD,
                                                       (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

void gst_gva_detect_init(GstGvaDetect *gvadetect) {
    GST_DEBUG_OBJECT(gvadetect, "gst_gva_detect_init");
    GST_DEBUG_OBJECT(gvadetect, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvadetect)));

    gvadetect->threshold = DEFALUT_THRESHOLD;

    PostProcFunction post_proc = ExtractBoundingBoxes;
    gvadetect->base_inference.post_proc = (void *)post_proc;
}

} // extern "C"
