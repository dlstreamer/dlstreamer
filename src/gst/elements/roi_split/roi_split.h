/*******************************************************************************
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <string>
#include <vector>

G_BEGIN_DECLS

#define DLS_BUFFER_FLAG_LAST_ROI_ON_FRAME (GST_BUFFER_FLAG_LAST << 1)

#define ROI_SPLIT_NAME "Split buffer with multiple GstVideoRegionOfInterestMeta into multiple buffers"
#define ROI_SPLIT_DESCRIPTION ROI_SPLIT_NAME

#define GST_TYPE_ROI_SPLIT (roi_split_get_type())
#define ROI_SPLIT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_ROI_SPLIT, RoiSplit))
#define ROI_SPLIT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_ROI_SPLIT, RoiSplitClass))
#define GST_IS_ROI_SPLIT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_ROI_SPLIT))
#define GST_IS_ROI_SPLIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_ROI_SPLIT))

typedef struct _RoiSplit {
    GstBaseTransform base;
    std::vector<std::string> object_classes;
} RoiSplit;

typedef struct _RoiSplitClass {
    GstBaseTransformClass base_class;
} RoiSplitClass;

GType roi_split_get_type(void);

G_END_DECLS
