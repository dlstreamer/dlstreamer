/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include <string>

#include <gvatensortometa.hpp>
#include <gvavideototensor.hpp>

G_BEGIN_DECLS

#define GVA_DETECT_BIN_NAME "[Preview] GVA Detect Bin"
#define GVA_DETECT_BIN_DESCRIPTION "Infrastructure to perform detect inference"

GST_DEBUG_CATEGORY_EXTERN(gva_detect_bin_debug_category);
#define GST_DEBUG_CAT_GVA_DETECT_BIN gva_detect_bin_debug_category

#define GST_TYPE_GVA_DETECT_BIN (gva_detect_bin_get_type())
#define GVA_DETECT_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_DETECT_BIN, GvaDetectBin))
#define GVA_DETECT_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_DETECT_BIN, GvaDetectBinClass))
#define GST_IS_GVA_DETECT_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_DETECT_BIN))
#define GST_IS_GVA_DETECT_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_DETECT_BIN))

typedef struct _GvaDetectBin {
    GstBin parent;

    GstPad *srcpad;
    GstPad *sinkpad;

    GstElement *tee;
    GstElement *queue1;
    GstElement *queue2;
    GstElement *preproc;
    GstElement *inference;
    GstElement *postproc;
    GstElement *aggregate;

    struct _Props {
        /* inference */
        std::string model;
        std::string ie_config;
        std::string device;
        std::string instance_id;
        guint nireq;
        guint batch_size;
        guint interval;
        /* pre-post-proc */
        std::string model_proc;
        PreProcBackend pre_proc_backend;
        post_processing::ConverterType converter_type;
        bool roi_list;
    } props;
} GvaDetectBin;

typedef struct _GvaDetectBinClass {
    GstBinClass parent_class;
} GvaDetectBinClass;

GType gva_detect_bin_get_type(void);

G_END_DECLS
