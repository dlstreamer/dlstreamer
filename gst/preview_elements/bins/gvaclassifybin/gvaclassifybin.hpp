/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gvainferencebin.hpp>

#include <string>

G_BEGIN_DECLS

#define GVA_CLASSIFY_BIN_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define GVA_CLASSIFY_BIN_DESCRIPTION                                                                                   \
    "Performs object classification. Accepts the ROI or full frame as an input and "                                   \
    "outputs classification results with metadata."

GST_DEBUG_CATEGORY_EXTERN(gva_classify_bin_debug_category);
#define GST_DEBUG_CAT_GVA_CLASSIFY_BIN gva_classify_bin_debug_category

#define GST_TYPE_GVA_CLASSIFY_BIN (gva_classify_bin_get_type())
#define GVA_CLASSIFY_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_CLASSIFY_BIN, GvaClassifyBin))
#define GVA_CLASSIFY_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_CLASSIFY_BIN, GvaClassifyBinClass))
#define GST_IS_GVA_CLASSIFY_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_CLASSIFY_BIN))
#define GST_IS_GVA_CLASSIFY_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_CLASSIFY_BIN))

typedef struct _GvaClassifyBin {
    GvaInferenceBin base;

    class GvaClassifyBinPrivate *impl;
} GvaClassifyBin;

typedef struct _GvaClassifyBinClass {
    GvaInferenceBinClass base;
} GvaClassifyBinClass;

GType gva_classify_bin_get_type(void);

G_END_DECLS
