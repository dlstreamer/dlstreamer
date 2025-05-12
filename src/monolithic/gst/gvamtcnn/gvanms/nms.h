/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvanms.h"
#include "gva_tensor_meta.h"

G_BEGIN_DECLS

typedef enum {
    NMS_MIN = 0,
    NMS_UNION,
} NMSMode;

gboolean non_max_suppression(GstGvaNms *nms, GstBuffer *buffer);

G_END_DECLS
