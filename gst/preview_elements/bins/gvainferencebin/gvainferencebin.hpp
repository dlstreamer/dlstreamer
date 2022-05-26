/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>
#include <list>
#include <string>

#include <gvatensortometa.hpp>

G_BEGIN_DECLS

#define GVA_INFERENCE_BIN_NAME "Generic full-frame inference (generates GstGVATensorMeta)"
#define GVA_INFERENCE_BIN_DESCRIPTION "Runs deep learning inference using any model with an RGB or BGR input."

GST_DEBUG_CATEGORY_EXTERN(gva_inference_bin_debug_category);
#define GST_DEBUG_CAT_GVA_INFERENCE_BIN gva_inference_bin_debug_category

#define GST_TYPE_GVA_INFERENCE_BIN (gva_inference_bin_get_type())
#define GVA_INFERENCE_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_INFERENCE_BIN, GvaInferenceBin))
#define GVA_INFERENCE_BIN_CLASS(klass)                                                                                 \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_INFERENCE_BIN, GvaInferenceBinClass))
#define GST_IS_GVA_INFERENCE_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_INFERENCE_BIN))
#define GST_IS_GVA_INFERENCE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_INFERENCE_BIN))
#define GVA_INFERENCE_BIN_GET_CLASS(obj)                                                                               \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_GVA_INFERENCE_BIN, GvaInferenceBinClass))

enum class PreProcessBackend : int {
    AUTO = 0,
    IE = 1,
    GST = 2,
    VAAPI = 3,
    VAAPI_OPENCL = 4,
    VAAPI_SURFACE_SHARING = 5,
    OPENCV_LEGACY = 6
};

enum class Region { FULL_FRAME, ROI_LIST };

typedef struct _GvaInferenceBin {
    GstBin base;

    class GvaInferenceBinPrivate *impl;
    /* member functions */
    PreProcessBackend get_pre_proc_type(GstCaps *);
    void set_converter_type(post_processing::ConverterType type);
} GvaInferenceBin;

typedef struct _GvaInferenceBinClass {
    GstBinClass base;

    bool (*init_preprocessing)(GstBin *bin, PreProcessBackend linkage, std::list<GstElement *> &link_order);
    GstElement *(*init_postprocessing)(GstBin *bin);
    GstElement *(*create_legacy_element)(GstBin *bin);
} GvaInferenceBinClass;

GType gva_inference_bin_get_type(void);

#define GST_TYPE_GVA_INFERENCE_BIN_BACKEND (gva_inference_bin_backend_get_type())
GType gva_inference_bin_backend_get_type(void);

#define GST_TYPE_GVA_INFERENCE_BIN_REGION (gva_inference_bin_region_get_type())
GType gva_inference_bin_region_get_type();
GstElement *create_element(const std::string &name, const std::map<std::string, std::string> &props = {});

G_END_DECLS
