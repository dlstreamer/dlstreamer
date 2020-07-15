/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef GVA_UTILS_H
#define GVA_UTILS_H

#include "glib.h"
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

G_BEGIN_DECLS

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id);
void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id);

G_END_DECLS

#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#endif // GVA_UTILS_H
