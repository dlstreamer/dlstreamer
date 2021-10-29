/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef GVA_UTILS_H
#define GVA_UTILS_H

#include "glib.h"
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/video/gstvideometa.h>

G_BEGIN_DECLS

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id);
void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id);

void gva_buffer_check_and_make_writable(GstBuffer **buffer, const char *called_function_name);

int gst_format_to_fourcc(int format);

G_END_DECLS

#define GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buf, state)                                                          \
    ((GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta_filtered(buf, state,                                      \
                                                                      GST_VIDEO_REGION_OF_INTEREST_META_API_TYPE))

#endif // GVA_UTILS_H
