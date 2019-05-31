/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "inference_singleton.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_BASE_INFERENCE (gva_base_inference_get_type())
#define GVA_BASE_INFERENCE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_BASE_INFERENCE, GvaBaseInference))

typedef struct _GvaBaseInference {
    GstBaseTransform base_gvaclassify;

    // properties
    gchar *model;
    gchar *object_class;
    gchar *model_proc;
    gchar *device;
    guint batch_size;
    guint every_nth_frame;
    guint nireq;
    gchar *inference_id;
    gchar *cpu_streams;
    gchar *infer_config;
    gchar *allocator_name;
    // other fields
    GstVideoInfo *info;
    gboolean is_full_frame;
    void *inference;        // C++ type: InferenceImpl*
    void *pre_proc;         // C++ type: PostProcFunction
    void *post_proc;        // C++ type: PostProcFunction
    void *get_roi_pre_proc; // C++ type: GetROIPreProcFunction
    gboolean initialized;
} GvaBaseInference;

typedef struct _GvaBaseInferenceClass {
    GstBaseTransformClass base_transform_class;
} GvaBaseInferenceClass;

GType gva_base_inference_get_type(void);

void gva_base_inference_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
void gva_base_inference_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
void gva_base_inference_dispose(GObject *object);
void gva_base_inference_finalize(GObject *object);

gboolean gva_base_inference_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);

gboolean gva_base_inference_start(GstBaseTransform *trans);
gboolean gva_base_inference_stop(GstBaseTransform *trans);
gboolean gva_base_inference_sink_event(GstBaseTransform *trans, GstEvent *event);

GstFlowReturn gva_base_inference_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
void gva_base_inference_cleanup(GvaBaseInference *gvaclassify);
GstStateChangeReturn gva_base_inference_change_state(GstElement *element, GstStateChange transition);

G_END_DECLS

#ifdef __cplusplus

#include "inference_backend/image_inference.h"
#include <functional>

typedef struct {
    GstBuffer *buffer;
    GstVideoRegionOfInterestMeta roi;
} InferenceROI;

class InferenceImpl;

// Pre-processing and post-processing function signatures
typedef void (*PreProcFunction)(GstStructure *preproc, InferenceBackend::Image &image);
typedef std::function<void(InferenceBackend::Image &)> (*GetROIPreProcFunction)(GstStructure *preproc,
                                                                                GstVideoRegionOfInterestMeta *roi_meta);
typedef void (*PostProcFunction)(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                 std::vector<InferenceROI> frames,
                                 const std::map<std::string, GstStructure *> &model_proc, const gchar *model_name,
                                 GvaBaseInference *gva_base_inference);

#endif /* __cplusplus */
