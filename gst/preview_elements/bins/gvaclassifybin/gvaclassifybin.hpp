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

#define GVA_CLASSIFY_BIN_NAME "[Preview] GVA Classify Bin"
#define GVA_CLASSIFY_BIN_DESCRIPTION "Infrastructure to perform classify inference"

GST_DEBUG_CATEGORY_EXTERN(gva_classify_bin_debug_category);
#define GST_DEBUG_CAT_GVA_CLASSIFY_BIN gva_classify_bin_debug_category

#define GST_TYPE_GVA_CLASSIFY_BIN (gva_classify_bin_get_type())
#define GVA_CLASSIFY_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_CLASSIFY_BIN, GvaClassifyBin))
#define GVA_CLASSIFY_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_CLASSIFY_BIN, GvaClassifyBinClass))
#define GST_IS_GVA_CLASSIFY_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_CLASSIFY_BIN))
#define GST_IS_GVA_CLASSIFY_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_CLASSIFY_BIN))

typedef struct _GvaClassifyBin {
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
        /* Aux */
        std::string object_class;
    } props;
} GvaClassifyBin;

typedef struct _GvaClassifyBinClass {
    GstBinClass parent_class;
} GvaClassifyBinClass;

GType gva_classify_bin_get_type(void);

G_END_DECLS
