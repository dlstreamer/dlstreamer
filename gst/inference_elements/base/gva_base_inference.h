/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "inference_singleton.h"
#include "processor_types.h"

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
    gchar *model_proc;
    gchar *device;
    guint batch_size;
    guint every_nth_frame;
    gboolean adaptive_skip;
    guint nireq;
    gchar *inference_id;
    guint cpu_streams;
    guint gpu_streams;
    gchar *infer_config;
    gchar *allocator_name;
    gchar *pre_proc_name;

    // other fields
    GstVideoInfo *info;
    gboolean is_full_frame;

    InferenceImpl *inference;

    IsROIClassificationNeededFunction is_roi_classification_needed;
    PreProcFunction pre_proc;
    GetROIPreProcFunction get_roi_pre_proc;
    PostProcFunction post_proc;

    gboolean initialized;
    guint num_skipped_frames;
} GvaBaseInference;

typedef struct _GvaBaseInferenceClass {
    GstBaseTransformClass base_transform_class;
} GvaBaseInferenceClass;

GType gva_base_inference_get_type(void);

G_END_DECLS
