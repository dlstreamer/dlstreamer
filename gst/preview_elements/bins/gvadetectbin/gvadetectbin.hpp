/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gvainferencebin.hpp>

#include <string>

G_BEGIN_DECLS

#define GVA_DETECT_BIN_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define GVA_DETECT_BIN_DESCRIPTION                                                                                     \
    "Performs object detection using SSD-like "                                                                        \
    "(including MobileNet-V1/V2 and ResNet), YoloV2/YoloV3/YoloV2-tiny/YoloV3-tiny "                                   \
    "and FasterRCNN-like object detection models."

GST_DEBUG_CATEGORY_EXTERN(gva_detect_bin_debug_category);
#define GST_DEBUG_CAT_GVA_DETECT_BIN gva_detect_bin_debug_category

#define GST_TYPE_GVA_DETECT_BIN (gva_detect_bin_get_type())
#define GVA_DETECT_BIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_DETECT_BIN, GvaDetectBin))
#define GVA_DETECT_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_DETECT_BIN, GvaDetectBinClass))
#define GST_IS_GVA_DETECT_BIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_DETECT_BIN))
#define GST_IS_GVA_DETECT_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_DETECT_BIN))

typedef struct _GvaDetectBin {
    GvaInferenceBin base;

    class GvaDetectBinPrivate *impl;
} GvaDetectBin;

typedef struct _GvaDetectBinClass {
    GvaInferenceBinClass base;
} GvaDetectBinClass;

GType gva_detect_bin_get_type(void);

G_END_DECLS
