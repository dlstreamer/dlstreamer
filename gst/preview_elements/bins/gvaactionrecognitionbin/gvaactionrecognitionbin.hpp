/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include <string>

#include <gvavideototensor.hpp>

G_BEGIN_DECLS

#define GVA_ACTION_RECOGNITION_BIN_NAME "[Preview] GVA Action Recognition Bin"
#define GVA_ACTION_RECOGNITION_BIN_DESCRIPTION "Infrastructure to perform action recognition inference"

GST_DEBUG_CATEGORY_EXTERN(gva_action_recognition_bin_debug_category);
#define GST_DEBUG_CAT_GVA_ACTION_RECOGNITION_BIN gva_action_recognition_bin_debug_category

#define GST_TYPE_GVA_ACTION_RECOGNITION_BIN (gva_action_recognition_bin_get_type())
#define GVA_ACTION_RECOGNITION_BIN(obj)                                                                                \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_ACTION_RECOGNITION_BIN, GvaActionRecognitionBin))
#define GVA_ACTION_RECOGNITION_BIN_CLASS(klass)                                                                        \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_ACTION_RECOGNITION_BIN, GvaActionRecognitionBinClass))
#define GST_IS_GVA_ACTION_RECOGNITION_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_ACTION_RECOGNITION_BIN))
#define GST_IS_GVA_ACTION_RECOGNITION_BIN_CLASS(klass)                                                                 \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_ACTION_RECOGNITION_BIN))

typedef struct _GvaActionRecognitionBin {
    GstBin parent;

    GstPad *srcpad;
    GstPad *sinkpad;

    GstElement *tee;
    GstElement *queue1;
    GstElement *queue2;
    GstElement *queue3;
    GstElement *preproc;
    GstElement *encoder_inference;
    GstElement *acc;
    GstElement *decoder_inference;
    GstElement *postproc;
    GstElement *aggregate;

    struct _Props {
        /* encoder */
        std::string enc_model;
        std::string enc_ie_config;
        std::string enc_device;
        guint enc_nireq;
        guint enc_batch_size;
        /* decoder */
        std::string dec_model;
        std::string dec_ie_config;
        std::string dec_device;
        guint dec_nireq;
        guint dec_batch_size;
        /* pre-post-proc */
        std::string model_proc;
        double threshold;
        PreProcBackend pre_proc_backend;
    } props;
} GvaActionRecognitionBin;

typedef struct _GvaActionRecognitionBinClass {
    GstBinClass parent_class;
} GvaActionRecognitionBinClass;

GType gva_action_recognition_bin_get_type(void);

G_END_DECLS
