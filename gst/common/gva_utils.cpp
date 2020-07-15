/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_utils.h"

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id) {
    GstStructure *object_id = gst_video_region_of_interest_meta_get_param(meta, "object_id");
    return object_id && gst_structure_get_int(object_id, "id", id);
}

void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id) {
    GstStructure *object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
    gst_video_region_of_interest_meta_add_param(meta, object_id);
}
